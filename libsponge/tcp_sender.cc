#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retransmission_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _recv_ackno; }

void TCPSender::fill_window() {
    TCPSegment seg;

    // 接收方发出的首个段应是只包含SYN的段，用作第一次握手。课本里是这么讲的，但讲义里没说
    if (!_syn_flag) {  // 发送syn段
        seg.header().syn = true;
        _syn_flag = true;
        send_segment(seg);
        return;
    }

    // take window_size as 1 when it equal 0
    // 心跳包  如果接收方窗口满了 你这里也是0 不发送东西的话,
    // 那么接收方永远不会发送ack 就卡死了 所以必须发送一字节的内容
    if (_window_size == 0) {
        _window_size = 1;
    }

    while (bytes_in_flight() < _window_size && !_fin_flag) {
        size_t len = min(_window_size - bytes_in_flight(), TCPConfig::MAX_PAYLOAD_SIZE);

        Buffer payload(_stream.read(len));  // seg.payload()是buffer 不是string  // 其实可以自动强转也不用管
        seg.payload() = payload;

        // 这里我之前是判断_stream关闭输入流_stream.input_ended(), 错了 其实应该判断eof(), 查看源代可以看出区别
        if (seg.length_in_sequence_space() + bytes_in_flight() < _window_size && _stream.eof()) {
            // 因为这里是用_window_size - _bytes_in_flight()填充的, 可能已经填不了fin了,
            seg.header().fin = true;
            _fin_flag = true;
        }

        // 等于0 的时候代表最后发了fin之后 还在发空的
        if (seg.length_in_sequence_space() == 0) {
            break;
        }

        send_segment(seg);
    }
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = wrap(_next_seqno, _isn);

    _segments_out.push(seg);
    _segments_outstanding.push(seg);

    _next_seqno += seg.length_in_sequence_space();
}
//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 这里checkpoint 应该用 _recv_ackno 而不是_next_seqno;  之前写错了  不过感觉没区别  这两者相差应该不会大于一轮

    uint64_t abs_ackno = unwrap(ackno, _isn, _recv_ackno);
    // out of window, invalid ackno
    if (abs_ackno > _next_seqno) {
        return false;
    }

    // if ackno is legal, modify _window_size before return
    _window_size = window_size;

    // ack has been received
    if (abs_ackno <= _recv_ackno) {
        return true;
    }

    _recv_ackno = abs_ackno;

    // pop all elment before ackno
    while (!_segments_outstanding.empty()) {
        TCPSegment seg = _segments_outstanding.front();
        if (unwrap(seg.header().seqno, _isn, _recv_ackno) + seg.length_in_sequence_space() <= _recv_ackno) {
            _segments_outstanding.pop();
        } else {
            break;
        }
    }

    // 这里我一开始写了如果!_segments_outstanding.empty()才重置 应该收到ackno就重置
    _timer = 0;
    _retransmission_timeout = _initial_retransmission_timeout;
    _consecutive_retransmission = 0;
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_segments_outstanding.empty()) {  // 没有任何未确认的  没发 那肯定不计时啊
        return;
    }

    _timer += ms_since_last_tick;
    if (_timer >= _retransmission_timeout) {
        _segments_out.push(_segments_outstanding.front());

        _timer = 0;
        _retransmission_timeout *= 2;
        _consecutive_retransmission++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmission; }

void TCPSender::send_empty_segment() {
    // empty segment doesn't need store to outstanding queue
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
