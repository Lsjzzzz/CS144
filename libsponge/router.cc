#include "router.hh"

#include <iostream>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";

    // 去掉不需要的后缀  然后后缀全加上0
    uint32_t real_ip = route_prefix >> (32 - prefix_length) << (32 - prefix_length);
    _route_mps[prefix_length][real_ip] = make_pair(next_hop, interface_num);
}

// 数据结构定义:
//                 prefix_length              route_prefix         next_hop   interface_num
//  std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::pair<Address, size_t>>> route_mps;

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    // 判断数据表是否存活
    if (dgram.header().ttl == 0 || --dgram.header().ttl == 0) {
        return;
    }
    uint32_t dst = dgram.header().dst;
    for (int len = 32; len >= 0; len--) {
        if (len == 0) {
            dst = 0;
        } else {
            dst = dst >> (32 - len) << (32 - len);
        }
        if (_route_mps[len].count(dst)) {
            auto [next_hop, interface_num] = _route_mps[len][dst];
            Address real_next_hop =
                next_hop.has_value() ? next_hop.value() : Address::from_ipv4_numeric(dgram.header().dst);
            interface(interface_num).send_datagram(dgram, real_next_hop);
            return;
        }
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
