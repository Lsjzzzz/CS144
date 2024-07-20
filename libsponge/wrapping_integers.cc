#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
// 无符号溢出等于取模
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t ret;

    uint32_t offset = n - isn;
    //这里恰好需要这个性质  天才
    //无符号整数的例子：
    // 假设 ( a = 10 ) 和 ( b = 5 )，这里我们使用无符号整数。
    // ( a - b = 10 - 5 = 5 )
    // ( b - a = 5 - 10 = 4294967291 )
    // 在无符号整数下，( b - a ) 的计算结果是一个大的正数，
    //因为在无符号整数中，减法会对结果进行模运算（即结果会循环回到 0），因此结果不同于 ( a - b )。
    ret = (checkpoint & 0xFFFFFFFF00000000) + offset;
    // 32位与 前32位1后32位0 相与  得到最近的数
    //用n的绝对序号 + -一个循环 三个数相比较
    uint64_t temp = ret;

    if (abs(int64_t(temp + (1ul << 32) - checkpoint)) < abs(int64_t(temp - checkpoint))) {
        ret = temp + (1ul << 32);
    }
    
    if (temp >= (1ul << 32) && abs(int64_t(temp - (1ul << 32) - checkpoint)) < abs(int64_t(ret - checkpoint))) {
        ret = temp - (1ul << 32);
    }

    return ret;
}
