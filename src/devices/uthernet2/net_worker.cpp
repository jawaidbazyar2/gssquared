/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Network worker thread: host sockets + libslirp. Never called from emu thread.
 */

#include "net_worker.hpp"

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
#define sock_error() WSAGetLastError()
#define SOCK_EINPROGRESS WSAEINPROGRESS
#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_EAGAIN WSAEWOULDBLOCK
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#define sock_error() errno
#define SOCK_EINPROGRESS EINPROGRESS
#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_EAGAIN EAGAIN
#endif

namespace u2 {
namespace {

#ifdef _WIN32
using socket_t = SOCKET;
constexpr socket_t kInvalid = INVALID_SOCKET;
void closesock(socket_t s) { closesocket(s); }
bool set_nonblocking(socket_t fd) {
    u_long on = 1;
    return ioctlsocket(fd, FIONBIO, &on) == 0;
}
#else
using socket_t = int;
constexpr socket_t kInvalid = -1;
void closesock(socket_t s) { close(s); }
bool set_nonblocking(socket_t fd) {
    const int cur = fcntl(fd, F_GETFL);
    return fcntl(fd, F_SETFL, cur | O_NONBLOCK) == 0;
}
#endif

}  // namespace

NetWorker::NetWorker() = default;

NetWorker::~NetWorker() {
    stop();
}

bool NetWorker::start() {
    if (running_.load()) {
        return true;
    }
    wake_ = SDL_CreateSemaphore(0);
    if (!wake_) {
        return false;
    }
    running_.store(true);
    thread_ = SDL_CreateThread(thread_main, "uthernet2", this);
    if (!thread_) {
        running_.store(false);
        SDL_DestroySemaphore(wake_);
        wake_ = nullptr;
        return false;
    }
    return true;
}

void NetWorker::stop() {
    if (!running_.load() && !thread_) {
        return;
    }
    running_.store(false);
    NetRequest req;
    req.msg = NetMsg::Shutdown;
    requests_.send(std::move(req));
    if (wake_) {
        SDL_SignalSemaphore(wake_);
    }
    if (thread_) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
    if (wake_) {
        SDL_DestroySemaphore(wake_);
        wake_ = nullptr;
    }
}

bool NetWorker::post(NetRequest req) {
    if (!requests_.send(std::move(req))) {
        fprintf(stderr, "Uthernet II: request ring full\n");
        return false;
    }
    wake();
    return true;
}

bool NetWorker::poll_event(NetEvent *out) {
    return events_.get(out);
}

void NetWorker::wake() {
    if (wake_) {
        SDL_SignalSemaphore(wake_);
    }
}

int NetWorker::thread_main(void *userdata) {
    static_cast<NetWorker *>(userdata)->run();
    return 0;
}

void NetWorker::post_event(NetEvent ev) {
    if (!events_.send(std::move(ev))) {
        fprintf(stderr, "Uthernet II: event ring full (dropping)\n");
    }
}

void NetWorker::run() {
#ifdef _WIN32
    WSADATA wsa{};
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    const SlirpNetConfig cfg = pick_slirp_network();
    slirp_ok_ = slirp_.init(cfg);
    slirp_.set_rx_callback([this](const uint8_t *pkt, size_t len) {
        NetEvent ev;
        ev.evt = NetEvt::MacRawRx;
        ev.sock = 0;
        ev.payload.assign(pkt, pkt + len);
        post_event(std::move(ev));
    });

    while (running_.load(std::memory_order_acquire)) {
        // Wait briefly for wake or timeout to poll sockets/slirp.
        SDL_WaitSemaphoreTimeout(wake_, 5);

        NetRequest req;
        while (requests_.get(&req)) {
            if (req.msg == NetMsg::Shutdown) {
                goto done;
            }
            handle_request(req);
        }
        poll_sockets();
        if (slirp_ok_) {
            slirp_.poll(0);
        }
    }

done:
    for (auto &s : socks_) {
        if (s.fd != kInvalid) {
            closesock(s.fd);
            s.fd = kInvalid;
        }
        s.status = W5100_SN_SR_CLOSED;
    }
    slirp_.shutdown();
#ifdef _WIN32
    WSACleanup();
#endif
}

void NetWorker::handle_request(const NetRequest &req) {
    if (req.sock >= U2_NUM_SOCKETS) {
        return;
    }
    SockState &st = socks_[req.sock];

    switch (req.msg) {
    case NetMsg::OpenTcp: {
        if (st.fd != kInvalid) {
            closesock(st.fd);
            st.fd = kInvalid;
        }
        socket_t fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd == kInvalid || !set_nonblocking(fd)) {
            if (fd != kInvalid) {
                closesock(fd);
            }
            st.status = W5100_SN_SR_CLOSED;
            post_event(NetEvent{NetEvt::Status, req.sock, W5100_SN_SR_CLOSED, 0, 0, 0, {}});
            break;
        }
        st.fd = fd;
        st.is_tcp = true;
        st.is_udp = false;
        st.listening = false;
        st.status = W5100_SN_SR_SOCK_INIT;
        post_event(NetEvent{NetEvt::Status, req.sock, st.status, 0, 0, 0, {}});
        break;
    }
    case NetMsg::OpenUdp: {
        if (st.fd != kInvalid) {
            closesock(st.fd);
            st.fd = kInvalid;
        }
        socket_t fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd == kInvalid || !set_nonblocking(fd)) {
            if (fd != kInvalid) {
                closesock(fd);
            }
            st.status = W5100_SN_SR_CLOSED;
            post_event(NetEvent{NetEvt::Status, req.sock, W5100_SN_SR_CLOSED, 0, 0, 0, {}});
            break;
        }
        if (req.local_port != 0) {
            sockaddr_in bind_addr{};
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            bind_addr.sin_port = htons(req.local_port);
            ::bind(fd, reinterpret_cast<sockaddr *>(&bind_addr), sizeof(bind_addr));
        }
        st.fd = fd;
        st.is_tcp = false;
        st.is_udp = true;
        st.listening = false;
        st.status = W5100_SN_SR_SOCK_UDP;
        post_event(NetEvent{NetEvt::Status, req.sock, st.status, 0, 0, 0, {}});
        break;
    }
    case NetMsg::Listen: {
        if (st.fd == kInvalid || !st.is_tcp) {
            break;
        }
        sockaddr_in bind_addr{};
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        bind_addr.sin_port = htons(req.local_port);
        ::bind(st.fd, reinterpret_cast<sockaddr *>(&bind_addr), sizeof(bind_addr));
        ::listen(st.fd, 1);
        st.listening = true;
        st.status = W5100_SN_SR_SOCK_LISTEN;
        post_event(NetEvent{NetEvt::Status, req.sock, st.status, 0, 0, 0, {}});
        break;
    }
    case NetMsg::Connect: {
        if (st.fd == kInvalid || !st.is_tcp) {
            break;
        }
        sockaddr_in dest{};
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = req.dest_ip;
        dest.sin_port = htons(req.dest_port);
        const int res = ::connect(st.fd, reinterpret_cast<sockaddr *>(&dest), sizeof(dest));
        if (res == 0) {
            st.status = W5100_SN_SR_ESTABLISHED;
        } else {
            const int err = sock_error();
            if (err == SOCK_EINPROGRESS || err == SOCK_EWOULDBLOCK) {
                st.status = W5100_SN_SR_SOCK_SYNSENT;
            } else {
                closesock(st.fd);
                st.fd = kInvalid;
                st.status = W5100_SN_SR_CLOSED;
            }
        }
        post_event(NetEvent{NetEvt::Status, req.sock, st.status, 0, 0, 0, {}});
        break;
    }
    case NetMsg::Close:
    case NetMsg::CloseHost: {
        if (st.fd != kInvalid) {
            closesock(st.fd);
            st.fd = kInvalid;
        }
        st.is_tcp = false;
        st.is_udp = false;
        st.listening = false;
        st.status = W5100_SN_SR_CLOSED;
        if (req.msg == NetMsg::Close) {
            post_event(NetEvent{NetEvt::Status, req.sock, st.status, 0, 0, 0, {}});
        }
        break;
    }
    case NetMsg::SendTcp: {
        if (st.fd != kInvalid && !req.payload.empty()) {
            ::send(st.fd, reinterpret_cast<const char *>(req.payload.data()),
                   static_cast<int>(req.payload.size()), 0);
        }
        break;
    }
    case NetMsg::SendUdp: {
        if (st.fd != kInvalid && !req.payload.empty()) {
            sockaddr_in dest{};
            dest.sin_family = AF_INET;
            dest.sin_addr.s_addr = req.dest_ip;
            dest.sin_port = htons(req.dest_port);
            ::sendto(st.fd, reinterpret_cast<const char *>(req.payload.data()),
                     static_cast<int>(req.payload.size()), 0, reinterpret_cast<sockaddr *>(&dest),
                     sizeof(dest));
        }
        break;
    }
    case NetMsg::MacRawTx:
    case NetMsg::IpRawTx: {
        if (slirp_ok_ && !req.payload.empty()) {
            slirp_.input(req.payload.data(), req.payload.size());
        }
        break;
    }
    default:
        break;
    }
}

void NetWorker::poll_sockets() {
    for (uint8_t i = 0; i < U2_NUM_SOCKETS; ++i) {
        SockState &st = socks_[i];
        if (st.fd == kInvalid) {
            continue;
        }

        if (st.status == W5100_SN_SR_SOCK_SYNSENT) {
#ifdef _WIN32
            fd_set wfds, efds;
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            FD_SET(st.fd, &wfds);
            FD_SET(st.fd, &efds);
            timeval tv{0, 0};
            if (select(0, nullptr, &wfds, &efds, &tv) > 0) {
#else
            pollfd pfd{st.fd, POLLOUT, 0};
            if (::poll(&pfd, 1, 0) > 0) {
#endif
                int err = 0;
                socklen_t elen = sizeof(err);
                getsockopt(st.fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &elen);
                if (err == 0) {
                    st.status = W5100_SN_SR_ESTABLISHED;
                } else {
                    closesock(st.fd);
                    st.fd = kInvalid;
                    st.status = W5100_SN_SR_CLOSED;
                }
                post_event(NetEvent{NetEvt::Status, i, st.status, 0, 0, 0, {}});
            }
        }

        if (st.listening && st.status == W5100_SN_SR_SOCK_LISTEN) {
            sockaddr_in peer{};
            socklen_t plen = sizeof(peer);
            socket_t client = ::accept(st.fd, reinterpret_cast<sockaddr *>(&peer), &plen);
            if (client != kInvalid) {
                set_nonblocking(client);
                closesock(st.fd);
                st.fd = client;
                st.listening = false;
                st.status = W5100_SN_SR_ESTABLISHED;
                post_event(NetEvent{NetEvt::Status, i, st.status, 0, 0, 0, {}});
            }
        }

        if (st.status == W5100_SN_SR_ESTABLISHED && st.is_tcp) {
            uint8_t buf[2048];
            const int n = ::recv(st.fd, reinterpret_cast<char *>(buf), sizeof(buf), 0);
            if (n > 0) {
                NetEvent ev;
                ev.evt = NetEvt::RxTcp;
                ev.sock = i;
                ev.payload.assign(buf, buf + n);
                post_event(std::move(ev));
            } else if (n == 0) {
                closesock(st.fd);
                st.fd = kInvalid;
                st.status = W5100_SN_SR_CLOSED;
                post_event(NetEvent{NetEvt::Status, i, st.status, 0, 0, 0, {}});
            }
        }

        if (st.status == W5100_SN_SR_SOCK_UDP && st.is_udp) {
            uint8_t buf[2048];
            sockaddr_in src{};
            socklen_t slen = sizeof(src);
            const int n = ::recvfrom(st.fd, reinterpret_cast<char *>(buf), sizeof(buf), 0,
                                     reinterpret_cast<sockaddr *>(&src), &slen);
            if (n > 0) {
                NetEvent ev;
                ev.evt = NetEvt::RxUdp;
                ev.sock = i;
                ev.src_ip = src.sin_addr.s_addr;
                ev.src_port = ntohs(src.sin_port);
                ev.payload.assign(buf, buf + n);
                post_event(std::move(ev));
            }
        }
    }
}

}  // namespace u2
