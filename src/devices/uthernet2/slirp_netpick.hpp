#pragma once

#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

namespace u2 {

struct SlirpNetConfig {
    struct in_addr vnetwork{};
    struct in_addr vnetmask{};
    struct in_addr vhost{};
    struct in_addr vdhcp_start{};
    struct in_addr vnameserver{};
};

/** Pick a /24 RFC1918 guest NAT that does not overlap host IPv4 interfaces. */
SlirpNetConfig pick_slirp_network();

}  // namespace u2
