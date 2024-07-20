#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

size_t StreamReassembler::merge_block(block &new_block, const block &old) {
    block block1 = new_block, block2 = old;
    if (block1.begin > block2.begin)
        swap(block1, block2);

    if (block1.begin + block1.length >= block2.begin + block2.length) {  // 包含关系
        new_block = block1;
        return block2.length;
    }

    // 交叉关系
    block1.data += block2.data.substr(block1.length + block1.begin - block2.begin);
    block1.length = block1.data.length();

    new_block = block1;
    return block1.length + block1.begin - block2.begin;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (index >= head_index + _capacity)
        return;  // 发送太后面的数据; asd asd

    block new_block;
    if (index + data.length() <= head_index)  // 发送已经读过的旧的   // 旧的应该不用判断eof吧?  他们都判断了
    {
        if (data.length() == 0 && index == head_index) {  // 接收到空数据的第一块是eof
            if (eof) {
                eof_flag = true;
            }

            if (eof_flag && empty()) {
                _output.end_input();
            }
        }
        return;
    } else if (index < head_index)  // 有新有旧
    {
        new_block.data.assign(data.begin() + head_index - index, data.end());
        new_block.length = new_block.data.length();
        new_block.begin = head_index;
    } else  // 纯新块
    {
        new_block.data = data;
        new_block.length = new_block.data.length();
        new_block.begin = index;
    }

    unassembled_byte += new_block.length;

    // merge substring
    size_t merged_bytes = 0;
    auto iter = blocks.lower_bound(new_block);
    while (iter != blocks.end() && (*iter).begin < new_block.begin + new_block.length) {  // 与后一块有交叉
        merged_bytes = merge_block(new_block, *iter);
        unassembled_byte -= merged_bytes;
        blocks.erase(iter);
        iter = blocks.lower_bound(new_block);
    }

    while (iter != blocks.begin()) {
        // 二分找到的不是第一块,如果是第一块, 前面就没了
        iter--;

        if ((*iter).begin + (*iter).length <= new_block.begin)
            break;  // 判断和前一块有没有交叉 如果没有break;

        merged_bytes = merge_block(new_block, *iter);
        unassembled_byte -= merged_bytes;
        blocks.erase(iter);
        iter = blocks.lower_bound(new_block);
    }

    blocks.insert(new_block);

    // write to ByteStream  第一块读入ByteStream
    while (!blocks.empty() && blocks.begin()->begin == head_index) {
        const block head_block = *blocks.begin();

        size_t write_bytes = _output.write(head_block.data);
        head_index += write_bytes;
        unassembled_byte -= write_bytes;
        blocks.erase(blocks.begin());
    }

    if (eof) {
        eof_flag = true;
    }

    if (eof_flag && empty()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return unassembled_byte; }

bool StreamReassembler::empty() const { return unassembled_byte == 0; }
