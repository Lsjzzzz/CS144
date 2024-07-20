#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    TCPHeader header = seg.header();
    if (header.syn) {
        if (syn_arrived) {
            return false;
        }  // already get a syn  , refuse other

        // get syn;
        ack_no = 1;
        syn_arrived = true;
        isn = seg.header().seqno;

        //! \brief Segment's length in sequence space
        //! \note Equal to payload length plus one byte if SYN is set, plus one byte if FIN is set
        // size_t length_in_sequence_space() const;
        if (seg.length_in_sequence_space() - 1 == 0) {  // segment's content only have a SYN flag
            return true;
        }

        // 头带数据;
        _reassembler.push_substring(seg.payload().copy(), 0, header.fin);

        ack_no = _reassembler.get_head_index() + 1;  // 因为syn不存在reassember中 所以序号要 + 1
        if (_reassembler.input_ended())              // FIN be count as one byte
            ack_no += 1;

        return true;

    } else if (!syn_arrived) {
        return false;  // before  get a SYN;
    }

    // uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint)
    uint64_t abs_seqno = unwrap(header.seqno, isn, ack_no);  // checkpoint 举例最近的 选择ack_no

    // 要判断在不在窗口中,  虽然调用_reassembler.push_substring他自己会判断的 所以直接push就行了
    // 但是这里函数返回值要求在窗口中才true
    if (abs_seqno >= window_size() + ack_no || abs_seqno + seg.length_in_sequence_space() <= ack_no) {  // 不在窗口中
        return false;
    }

    _reassembler.push_substring(seg.payload().copy(), abs_seqno - 1, header.fin);
    // abs_seqno - 1  和ack_no + 1 一样  syn不存在流索引中

    ack_no = _reassembler.get_head_index() + 1;  // 因为syn不存在reassember中 所以序号要 + 1
    if (_reassembler.input_ended())              // FIN be count as one byte
        ack_no += 1;
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!syn_arrived) {
        return std::nullopt;
    } else {
        return WrappingInt32(wrap(ack_no, isn));
    }
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
