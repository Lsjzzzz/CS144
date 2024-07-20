#include "byte_stream.hh"

#include <algorithm>
#include <iterator>
#include <stdexcept>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _error(false)
    , _capacity(capacity)
    , _head(0)
    , _tail(0)
    , _size(0)
    , _buf(capacity + 1, '0')
    , _read_bytes(0)
    , _write_bytes(0)
    , _end_input(false) {}

size_t ByteStream::write(const string &data) {
    size_t write_len = min(_capacity - _size, data.length());

    for (size_t i = 0; i < write_len; i++) {
        _buf[_tail++] = data[i];

        _tail %= _capacity;
    }
    _write_bytes += write_len;
    _size += write_len;

    return write_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string res;
    size_t peek_len = min(len, _size), idx = _head;
    while (peek_len--) {
        res += _buf[idx++];
        idx %= _capacity;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t pop_len = min(len, _size);
    _size -= pop_len;
    _head = (_head + pop_len) % _capacity;
    _read_bytes += pop_len;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return _size == 0; }

bool ByteStream::eof() const { return _end_input == true && _size == 0; }

size_t ByteStream::bytes_written() const { return _write_bytes; }

size_t ByteStream::bytes_read() const { return _read_bytes; }

size_t ByteStream::remaining_capacity() const { return _capacity - _size; }
