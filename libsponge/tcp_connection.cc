#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received_ms; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received_ms = 0;

    if (!_active) {
        return;
    }

    // 如果是 RST 包，则直接终止
    //! NOTE: 当 TCP 处于任何状态时，均需绝对接受 RST。因为这可以防止尚未到来数据包产生的影响
    if (seg.header().rst) {
        close();
        set_rst_state();
        return;
    }

    // 读取并处理接收到的数据
    // _receiver 足够鲁棒以至于无需进行任何过滤
    _receiver.segment_received(seg);

    // 如果是 LISEN 到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    // 需要发送空ack给对方
    // 如果此时 sender 发送了新数据，则停止发送空ack
    if (_receiver.ackno().has_value() && seg.header().seqno == _receiver.ackno().value() - 1 &&
        seg.length_in_sequence_space() == 0) {
        _sender.send_empty_segment();
        fill_window();
        return;
    }

    // 如果收到了 ACK 包，则更新 _sender 的状态并补充发送数据
    // NOTE: _sender 足够鲁棒以至于无需关注传入 ack 是否可靠
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    _sender.fill_window();
    fill_window();

    // 如果收到的不是 空seg 并且没有消息发送 必须要发送一个空seg
    if (seg.length_in_sequence_space() > 0 && _segments_out.empty()) {
        _sender.send_empty_segment();
        fill_window();
    }

    // 判断 TCP 断开连接时是否时需要等待  下面两种方法等价
    // if (!_sender.stream_in().eof() && _receiver.stream_out().input_ended()) {
    //     _linger_after_streams_finish = false;
    // }
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
    }

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        close();
        return;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (!_active) {
        return 0;
    }
    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    fill_window();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active) {
        return;
    }

    _sender.tick(ms_since_last_tick);
    fill_window();  // 感觉没必要啊
    _time_since_last_segment_received_ms += ms_since_last_tick;

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        close();
        send_rst_seg();
        set_rst_state();
        return;
    }

    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && _linger_after_streams_finish &&
        _time_since_last_segment_received_ms >= 10 * _cfg.rt_timeout) {
        close();
    }
}

void TCPConnection::end_input_stream() {
    //! \brief Shut down the outbound byte stream (still allows reading incoming data)
    _sender.stream_in().end_input();
    // 输入流结束后要发送fin
    _sender.fill_window();
    fill_window();
}

void TCPConnection::connect() {
    //! \brief Initiate a connection by sending a SYN segment
    _active = true;
    _sender.fill_window();
    fill_window();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            close();
            send_rst_seg();
            set_rst_state();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::set_rst_state() {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
}
void TCPConnection::send_rst_seg() {
    _sender.send_empty_segment();
    TCPSegment seg = _sender.segments_out().front();
    _sender.segments_out().pop();

    seg.header().rst = true;
    seg.header().ackno = _receiver.ackno().value();

    _segments_out.push(seg);
}
void TCPConnection::close() {
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    while (!_segments_out.empty()) {
        _segments_out.pop();
    }
    _active = false;
    _linger_after_streams_finish = false;
}
void TCPConnection::fill_window() {
    // 将等待发送的数据包加上本地的 ackno 和 window size
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size() > UINT16_MAX ? UINT16_MAX : _receiver.window_size();
        }
        _segments_out.push(seg);
    }
}
