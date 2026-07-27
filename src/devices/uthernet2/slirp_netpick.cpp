/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Choose a libslirp virtual /24 that does not overlap host networks.
 */

#include "slirp_netpick.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sys/types.h>
#endif

namespace u2 {
namespace {

struct HostNet {
    uint32_t network = 0; // host order
    uint32_t mask = 0;    // host order
};

bool ranges_overlap(uint32_t a_net, uint32_t a_mask, uint32_t b_net, uint32_t b_mask) {
    const uint32_t mask = a_mask & b_mask;
    return (a_net & mask) == (b_net & mask);
}

std::vector<HostNet> enumerate_host_nets() {
    std::vector<HostNet> out;
#ifdef _WIN32
    ULONG size = 0;
    GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &size);
    std::vector<uint8_t> buf(size ? size : 16 * 1024);
    auto *addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buf.data());
    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &size) != NO_ERROR) {
        return out;
    }
    for (auto *a = addrs; a; a = a->Next) {
        for (auto *u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            const auto *sin = reinterpret_cast<const sockaddr_in *>(u->Address.lpSockaddr);
            const uint32_t ip = ntohl(sin->sin_addr.s_addr);
            uint32_t m = 0;
            if (u->OnLinkPrefixLength <= 32) {
                m = u->OnLinkPrefixLength == 0 ? 0 : (0xFFFFFFFFu << (32 - u->OnLinkPrefixLength));
            }
            if (m == 0) {
                continue;
            }
            out.push_back(HostNet{ip & m, m});
        }
    }
#else
    struct ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) != 0) {
        return out;
    }
    for (struct ifaddrs *p = ifa; p != nullptr; p = p->ifa_next) {
        if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET || !p->ifa_netmask) {
            continue;
        }
        const auto *addr = reinterpret_cast<const sockaddr_in *>(p->ifa_addr);
        const auto *mask = reinterpret_cast<const sockaddr_in *>(p->ifa_netmask);
        const uint32_t ip = ntohl(addr->sin_addr.s_addr);
        const uint32_t m = ntohl(mask->sin_addr.s_addr);
        if (m == 0) {
            continue;
        }
        out.push_back(HostNet{ip & m, m});
    }
    freeifaddrs(ifa);
#endif
    return out;
}

bool conflicts(const std::vector<HostNet> &hosts, uint32_t cand_net_host, uint32_t cand_mask_host) {
    for (const auto &h : hosts) {
        if (ranges_overlap(h.network, h.mask, cand_net_host, cand_mask_host)) {
            return true;
        }
    }
    return false;
}

SlirpNetConfig make_config(uint32_t net_host) {
    SlirpNetConfig c;
    c.vnetwork.s_addr = htonl(net_host);
    c.vnetmask.s_addr = htonl(0xFFFFFF00u);
    c.vhost.s_addr = htonl(net_host + 2);
    c.vdhcp_start.s_addr = htonl(net_host + 15);
    c.vnameserver.s_addr = htonl(net_host + 3);
    return c;
}

}  // namespace

SlirpNetConfig pick_slirp_network() {
    const auto hosts = enumerate_host_nets();
    const uint32_t mask = 0xFFFFFF00u;

    std::vector<uint32_t> candidates;
    candidates.reserve(1024);
    // Prefer classic QEMU 10.0.2.0/24
    candidates.push_back(0x0A000200u);
    for (uint32_t x = 3; x < 256; ++x) {
        candidates.push_back(0x0A000000u | (x << 8)); // 10.0.x.0
    }
    for (uint32_t a = 1; a <= 10; ++a) {
        for (uint32_t b = 0; b < 256; ++b) {
            candidates.push_back(0x0A000000u | (a << 16) | (b << 8)); // 10.a.b.0
        }
    }
    for (uint32_t n = 16; n <= 31; ++n) {
        for (uint32_t h = 0; h < 16; ++h) {
            candidates.push_back(0xAC000000u | (n << 16) | (h << 8)); // 172.n.h.0
        }
    }
    for (int x = 254; x >= 0; --x) {
        candidates.push_back(0xC0A80000u | (static_cast<uint32_t>(x) << 8));
    }

    for (uint32_t net : candidates) {
        if (!conflicts(hosts, net, mask)) {
            return make_config(net);
        }
    }

    fprintf(stderr, "Uthernet II: no free RFC1918 /24 found; using 10.0.2.0/24\n");
    return make_config(0x0A000200u);
}

}  // namespace u2
