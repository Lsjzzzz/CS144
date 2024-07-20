#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    // 先查找 APR table
    const auto &it = _arp_table.find(next_hop_ip);
    // 如果 映射表里表里有的next_hop对应的ip，则直接发送；
    if (it != _arp_table.end()) {
        EthernetFrame frame;
        frame.header().dst = it->second.ethernet_address;
        frame.header().src = _ethernet_address; 
        frame.header().type = EthernetHeader::TYPE_IPv4;
        frame.payload() = dgram.serialize();

        _frames_out.push(frame);
        return;
    }

    // 映射表里表里一开始没有next_hop对应的ip

    // 等待 ARP 报文返回的待处理 IP 报文   中     没有这个arp请求    发送arp请求
    if (_waiting_arp_response_ip_addr.find(next_hop_ip) == _waiting_arp_response_ip_addr.end()) {
        ARPMessage arp_request;
        arp_request.opcode = ARPMessage::OPCODE_REQUEST;
        arp_request.sender_ethernet_address = _ethernet_address;
        arp_request.sender_ip_address = _ip_address.ipv4_numeric();
        arp_request.target_ethernet_address = {/* 这里应该置为空*/};
        arp_request.target_ip_address = next_hop_ip;

        EthernetFrame frame;
        frame.header().dst = ETHERNET_BROADCAST;
        frame.header().src = _ethernet_address;
        frame.header().type = EthernetHeader::TYPE_ARP;
        frame.payload() = arp_request.serialize();

        _frames_out.push(frame);

        _waiting_arp_response_ip_addr[next_hop_ip] = _arp_response_default_ttl;
    }

    // 将该 IP 包加入等待队列中
    _waiting_arp_internet_datagrams.push_back({next_hop, dgram});
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    // 如果不是发往当前位置的包
    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return std::nullopt;
    }
    // 如果是ip包
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        if (datagram.parse(frame.payload()) != ParseResult::NoError) {  // Parse 出错了
            return nullopt;
        }
        // 没出错返回
        return datagram;
    }

    // 其他情况下，是 ARP 包  显示写if表明一下
    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        // 将有效载荷解析为ARP消息
        ARPMessage arp_msg;
        if (arp_msg.parse(frame.payload()) != ParseResult::NoError) {  // Parse 出错了
            return nullopt;
        }
        // 获取相关的以太网地址和ip地址
        const uint32_t &src_ip_addr = arp_msg.sender_ip_address;
        // const uint32_t &dst_ip_addr = arp_msg.target_ip_address;  //未用
        const EthernetAddress &src_eth_addr = arp_msg.sender_ethernet_address;
        // const EthernetAddress &dst_eth_addr = arp_msg.target_ethernet_address; //未用

        // 如果是一个发给自己的 ARP 请求  ARP请求请求我们的IP地址，请发送适当的ARP回复。
        if ((arp_msg.opcode == ARPMessage::OPCODE_REQUEST) &&
            (arp_msg.target_ip_address == _ip_address.ipv4_numeric())) {
            ARPMessage arp_reply;
            arp_reply.opcode = arp_reply.OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp_msg.sender_ethernet_address;
            arp_reply.target_ip_address = arp_msg.sender_ip_address;

            // 发送
            EthernetFrame frame_reply;
            frame_reply.header().type = EthernetHeader::TYPE_ARP;
            frame_reply.header().dst = arp_msg.sender_ethernet_address;
            frame_reply.header().src = _ethernet_address;

            frame_reply.payload() = arp_reply.serialize();

            _frames_out.push(frame_reply);
        }

        //! NOTE: 我们可以同时从 ARP 请求和响应包中获取到新的 ARP 表项
        // 更新映射  不管是ARP 请求和响应包 都可以获取到新的 ARP 表项  所以都要将没发送的发送出去
        _arp_table[src_ip_addr] = {src_eth_addr, _arp_entry_default_ttl};
        // 将对应数据从原先等待队列里发送  删除
        for (auto it = _waiting_arp_internet_datagrams.begin(); it != _waiting_arp_internet_datagrams.end();) {
            Address address = it->first;
            InternetDatagram datagram = it->second;
            // 如果ip是更新的ip, 则发送
            if (address.ipv4_numeric() == src_ip_addr) {
                send_datagram(datagram, address);
                // 删除
                it = _waiting_arp_internet_datagrams.erase(it);
            } else {
                it++;
            }
        }
        // 删除已发送未回应的部分
        _waiting_arp_response_ip_addr.erase(src_ip_addr);
    }

    // 其他异常
    return nullopt;
}
//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    // 将 ARP 表中过期的条目删除
    for (auto it = _arp_table.begin(); it != _arp_table.end();) {
        it->second.ttl -= ms_since_last_tick;

        if (it->second.ttl <= 0)  // 过期
            it = _arp_table.erase(it);
        else {
            it++;
        }
    }
    // 将 ARP 等待队列中过期的条目删除
    for (auto it = _waiting_arp_response_ip_addr.begin(); it != _waiting_arp_response_ip_addr.end(); /* nop */) {
        it->second -= ms_since_last_tick;

        // 如果 ARP 等待队列中的 ARP 请求过期
        if (it->second <= 0) {
            // 重新发送 ARP 请求
            ARPMessage arp_request;
            arp_request.opcode = ARPMessage::OPCODE_REQUEST;
            arp_request.sender_ethernet_address = _ethernet_address;
            arp_request.sender_ip_address = _ip_address.ipv4_numeric();
            arp_request.target_ethernet_address = {/* 这里应该置为空*/};
            arp_request.target_ip_address = it->first;

            // 发送
            EthernetFrame frame;
            frame.header().dst = ETHERNET_BROADCAST;
            frame.header().src = _ethernet_address;
            frame.header().type = EthernetHeader::TYPE_ARP;

            frame.payload() = arp_request.serialize();

            _frames_out.push(frame);

            it->second = _arp_response_default_ttl;
        } else {
            it++;
        }
    }
}
