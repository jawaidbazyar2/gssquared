/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "serial_devices/netprinter/NetPrinterDevice.hpp"

#include "serial_devices/print/PrintTiming.hpp"

#include <cstring>
#include <utility>

#if defined(__EMSCRIPTEN__)

struct NetPrinterDevice::Impl {};

NetPrinterDevice::NetPrinterDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames,
                                   const char *port_id, const std::string &host, int tcp_port)
    : SerialDevice("NetPrinterDevice", port_id), host_(host), tcp_port_(tcp_port),
      event_queue_(event_queue), frames_(frames), impl_(std::make_unique<Impl>()) {
    if (frames_) {
        frame_handler_id_ = frames_->registerHandler([this]() {
            poll();
            return true;
        });
    }
    start_worker();
}

NetPrinterDevice::~NetPrinterDevice() {
    if (frames_ && frame_handler_id_ != static_cast<size_t>(-1)) {
        frames_->unregisterHandler(frame_handler_id_);
        frames_ = nullptr;
        frame_handler_id_ = static_cast<size_t>(-1);
    }
    if (thread) {
        SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
        q_host.send(msg);
        SDL_WaitThread(thread, nullptr);
        thread = nullptr;
    }
    event_queue_ = nullptr;
}

void NetPrinterDevice::device_loop() {
    while (true) {
        SDL_Delay(10);
        while (!q_host.is_empty()) {
            SerialMessage msg = q_host.get();
            if (msg.type == MESSAGE_SHUTDOWN) {
                return;
            }
        }
    }
}

void NetPrinterDevice::format_hud_status(char *buf, size_t n) const {
    format_handshake(buf, n);
}

#else

#include <cerrno>
#include <cstdio>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

#if defined(_WIN32)
using sock_t = SOCKET;
constexpr sock_t kInvalidSock = INVALID_SOCKET;
#else
using sock_t = int;
constexpr sock_t kInvalidSock = -1;
#endif

struct ResolvedAddr {
    sockaddr_storage ss{};
    socklen_t len = 0;
};

void close_sock(sock_t &fd) {
    if (fd == kInvalidSock) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif
    fd = kInvalidSock;
}

void set_cloexec(sock_t fd) {
#if defined(_WIN32)
    SetHandleInformation(reinterpret_cast<HANDLE>(fd), HANDLE_FLAG_INHERIT, 0);
#else
    int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
#endif
}

void set_nonblock(sock_t fd) {
#if defined(_WIN32)
    u_long mode = 1;
    ::ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void set_nosigpipe(sock_t fd) {
#if defined(SO_NOSIGPIPE)
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}

#if defined(_WIN32)
bool ensure_winsock() {
    static bool started = false;
    if (started) {
        return true;
    }
    WSADATA wsa{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    started = true;
    return true;
}
#endif

bool would_block() {
#if defined(_WIN32)
    const int err = ::WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

bool interrupted() {
#if defined(_WIN32)
    return ::WSAGetLastError() == WSAEINTR;
#else
    return errno == EINTR;
#endif
}

std::string endpoint_text(const std::string &host, int port) {
    if (host.find(':') != std::string::npos) {
        return "[" + host + "]:" + std::to_string(port);
    }
    return host + ":" + std::to_string(port);
}

struct ResolveJob {
    std::string host;
    std::string port;
    std::atomic<bool> done{false};
    int gai_error = 0;
    std::vector<ResolvedAddr> addrs;
};

int SDLCALL resolve_thread(void *data) {
    auto *job = static_cast<ResolveJob *>(data);
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo *res = nullptr;
    job->gai_error = ::getaddrinfo(job->host.c_str(), job->port.c_str(), &hints, &res);
    if (job->gai_error == 0 && res) {
        for (addrinfo *p = res; p != nullptr; p = p->ai_next) {
            if (!p->ai_addr || p->ai_addrlen == 0 ||
                p->ai_addrlen > sizeof(sockaddr_storage)) {
                continue;
            }
            ResolvedAddr a;
            std::memcpy(&a.ss, p->ai_addr, p->ai_addrlen);
            a.len = static_cast<socklen_t>(p->ai_addrlen);
            job->addrs.push_back(a);
        }
        ::freeaddrinfo(res);
    }
    job->done.store(true, std::memory_order_release);
    return 0;
}

} // namespace

enum class JobState { Idle, Resolving, Connecting, Open, Failed };

struct NetPrinterDevice::Impl {
    NetPrinterDevice *owner = nullptr;
    sock_t fd = kInvalidSock;
    JobState state = JobState::Idle;
    uint64_t last_write_ticks = 0;
    uint64_t connect_start_ticks = 0;
    bool toasted_this_job = false;
    std::vector<uint8_t> hold;
    std::vector<uint8_t> outbound;
    size_t out_off = 0;
    std::vector<uint8_t> inbound;
    size_t in_off = 0;
    std::unique_ptr<ResolveJob> resolve_job;
    SDL_Thread *resolve_thread_handle = nullptr;
    std::vector<ResolvedAddr> addrs;
    size_t addr_index = 0;

    ~Impl() { shutdown(); }

    void shutdown() {
        join_resolve();
        close_socket();
        hold.clear();
        outbound.clear();
        out_off = 0;
        inbound.clear();
        in_off = 0;
        state = JobState::Idle;
        last_write_ticks = 0;
        toasted_this_job = false;
        if (owner) {
            owner->set_socket_open(false);
        }
    }

    void join_resolve() {
        if (resolve_thread_handle) {
            SDL_WaitThread(resolve_thread_handle, nullptr);
            resolve_thread_handle = nullptr;
        }
        resolve_job.reset();
    }

    void close_socket() {
        close_sock(fd);
        if (owner) {
            owner->set_socket_open(false);
        }
    }

    void mark_write() { last_write_ticks = SDL_GetTicks(); }

    void toast(const char *fmt_host_port_msg, const std::string &host, int port) {
        if (toasted_this_job || !owner) {
            return;
        }
        toasted_this_job = true;
        char text[192];
        std::snprintf(text, sizeof(text), fmt_host_port_msg, endpoint_text(host, port).c_str());
        owner->post_toast(text);
    }

    void reap_resolve() {
        if (state != JobState::Resolving && resolve_job &&
            resolve_job->done.load(std::memory_order_acquire)) {
            join_resolve();
        }
    }

    void fail_reach(const std::string &host, int port) {
        SDL_Log("NetPrinter: could not reach %s", endpoint_text(host, port).c_str());
        close_socket();
        hold.clear();
        outbound.clear();
        out_off = 0;
        inbound.clear();
        in_off = 0;
        addrs.clear();
        addr_index = 0;
        state = JobState::Failed;
        mark_write();
        toast("Net Printer: could not reach %s", host, port);
        reap_resolve();
    }

    void go_idle() {
        join_resolve();
        close_socket();
        hold.clear();
        outbound.clear();
        out_off = 0;
        inbound.clear();
        in_off = 0;
        addrs.clear();
        addr_index = 0;
        state = JobState::Idle;
        last_write_ticks = 0;
        toasted_this_job = false;
    }

    void start_resolve(const std::string &host, int port) {
        join_resolve();
#if defined(_WIN32)
        if (!ensure_winsock()) {
            fail_reach(host, port);
            return;
        }
#endif
        auto job = std::make_unique<ResolveJob>();
        job->host = host;
        job->port = std::to_string(port);
        resolve_job = std::move(job);
        resolve_thread_handle =
            SDL_CreateThread(resolve_thread, "netprinter-dns", resolve_job.get());
        if (!resolve_thread_handle) {
            fail_reach(host, port);
            return;
        }
        state = JobState::Resolving;
        connect_start_ticks = SDL_GetTicks();
    }

    bool begin_connect_one() {
        while (addr_index < addrs.size()) {
            const ResolvedAddr &a = addrs[addr_index];
            ++addr_index;
            sock_t s = ::socket(a.ss.ss_family, SOCK_STREAM, IPPROTO_TCP);
            if (s == kInvalidSock) {
                continue;
            }
            set_cloexec(s);
            set_nonblock(s);
            set_nosigpipe(s);
            const int rc = ::connect(s, reinterpret_cast<const sockaddr *>(&a.ss), a.len);
            if (rc == 0) {
                fd = s;
                return true;
            }
            if (would_block()) {
                fd = s;
                return false;
            }
            close_sock(s);
        }
        return false;
    }

    void enter_open() {
        state = JobState::Open;
        if (owner) {
            SDL_Log("NetPrinter: connected to %s",
                    endpoint_text(owner->host(), owner->tcp_port()).c_str());
            owner->set_socket_open(true);
        }
        outbound.insert(outbound.end(), hold.begin(), hold.end());
        hold.clear();
        flush_out();
    }

    void poll_resolve(const std::string &host, int port) {
        if (state != JobState::Resolving || !resolve_job) {
            return;
        }
        if (!resolve_job->done.load(std::memory_order_acquire)) {
            if (SDL_GetTicks() - connect_start_ticks >= print_timing::kConnectTimeoutMs) {
                fail_reach(host, port);
            }
            return;
        }
        SDL_WaitThread(resolve_thread_handle, nullptr);
        resolve_thread_handle = nullptr;
        const int err = resolve_job->gai_error;
        addrs = std::move(resolve_job->addrs);
        resolve_job.reset();
        if (err != 0 || addrs.empty()) {
            fail_reach(host, port);
            return;
        }
        addr_index = 0;
        close_socket();
        connect_start_ticks = SDL_GetTicks();
        if (begin_connect_one()) {
            enter_open();
        } else if (fd != kInvalidSock) {
            state = JobState::Connecting;
        } else {
            fail_reach(host, port);
        }
    }

    void advance_or_fail_connect(const std::string &host, int port) {
        close_socket();
        if (begin_connect_one()) {
            enter_open();
        } else if (fd != kInvalidSock) {
            state = JobState::Connecting;
        } else {
            fail_reach(host, port);
        }
    }

    void poll_connect(const std::string &host, int port) {
        if (state != JobState::Connecting) {
            return;
        }
        if (fd == kInvalidSock) {
            fail_reach(host, port);
            return;
        }
        if (SDL_GetTicks() - connect_start_ticks >= print_timing::kConnectTimeoutMs) {
            fail_reach(host, port);
            return;
        }
        fd_set wfds;
        fd_set efds;
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        FD_SET(fd, &wfds);
        FD_SET(fd, &efds);
        timeval tv{};
        const int nfds = static_cast<int>(fd) + 1;
        const int rc = ::select(nfds, nullptr, &wfds, &efds, &tv);
        if (rc == 0) {
            return;
        }
        if (rc < 0) {
            if (interrupted()) {
                return;
            }
            advance_or_fail_connect(host, port);
            return;
        }
        // Read SO_ERROR once — getsockopt(SO_ERROR) clears the pending error on read.
        int soerr = 0;
        socklen_t len = sizeof(soerr);
        const int opt_rc =
            ::getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&soerr), &len);
        if (opt_rc == 0 && soerr == 0 && FD_ISSET(fd, &wfds)) {
            enter_open();
            return;
        }
        advance_or_fail_connect(host, port);
    }

    void flush_out() {
        while (fd != kInvalidSock && out_off < outbound.size()) {
            const char *p = reinterpret_cast<const char *>(outbound.data() + out_off);
            const size_t left = outbound.size() - out_off;
#if defined(_WIN32)
            const int n = ::send(fd, p, static_cast<int>(left), 0);
            if (n == SOCKET_ERROR) {
                if (would_block()) {
                    return;
                }
                note_peer_closed();
                return;
            }
#else
            const ssize_t n = ::send(fd, p, left,
#if defined(MSG_NOSIGNAL)
                                     MSG_NOSIGNAL
#else
                                     0
#endif
            );
            if (n < 0) {
                if (interrupted()) {
                    continue;
                }
                if (would_block()) {
                    return;
                }
                note_peer_closed();
                return;
            }
#endif
            if (n == 0) {
                note_peer_closed();
                return;
            }
            out_off += static_cast<size_t>(n);
        }
        if (out_off > 0 && out_off == outbound.size()) {
            outbound.clear();
            out_off = 0;
        }
    }

    void drain_in(SerialQueue &q_dev) {
        if (fd == kInvalidSock) {
            return;
        }
        for (;;) {
            while (in_off < inbound.size() && !q_dev.is_full()) {
                q_dev.send(SerialMessage{MESSAGE_DATA, inbound[in_off]});
                ++in_off;
            }
            if (in_off >= inbound.size()) {
                inbound.clear();
                in_off = 0;
            }
            if (q_dev.is_full()) {
                return;
            }
            char buf[256];
#if defined(_WIN32)
            const int n = ::recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                inbound.insert(inbound.end(), buf, buf + n);
                continue;
            }
            if (n < 0 && interrupted()) {
                continue;
            }
            if (n < 0 && would_block()) {
                return;
            }
            note_peer_closed();
            return;
#else
            const ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
            if (n > 0) {
                inbound.insert(inbound.end(), buf, buf + n);
                continue;
            }
            if (n < 0 && interrupted()) {
                continue;
            }
            if (n < 0 && would_block()) {
                return;
            }
            note_peer_closed();
            return;
#endif
        }
    }

    void note_peer_closed() {
        const std::string host = owner ? owner->host() : std::string{};
        const int port = owner ? owner->tcp_port() : 0;
        close_socket();
        hold.clear();
        outbound.clear();
        out_off = 0;
        inbound.clear();
        in_off = 0;
        if (state == JobState::Open) {
            toast("Net Printer: %s closed", host, port);
        }
        state = JobState::Idle;
        last_write_ticks = 0;
        toasted_this_job = false;
    }

    void handle_data(uint8_t byte, const std::string &host, int port) {
        switch (state) {
            case JobState::Idle:
                toasted_this_job = false;
                hold.clear();
                hold.push_back(byte);
                mark_write();
                start_resolve(host, port);
                break;
            case JobState::Resolving:
            case JobState::Connecting:
                hold.push_back(byte);
                mark_write();
                break;
            case JobState::Open:
                outbound.push_back(byte);
                mark_write();
                flush_out();
                break;
            case JobState::Failed:
                mark_write();
                break;
        }
    }

    void tick_idle() {
        if (state == JobState::Idle) {
            return;
        }
        if (last_write_ticks == 0) {
            return;
        }
        // Failed jobs clear after a short quiet gap so the next print session retries promptly.
        constexpr uint64_t kFailedIdleResetMs = 5'000;
        const uint64_t limit =
            (state == JobState::Failed) ? kFailedIdleResetMs : print_timing::kIdleCloseMs;
        if (SDL_GetTicks() - last_write_ticks >= limit) {
            if (state == JobState::Open && owner) {
                SDL_Log("NetPrinter: idle close %s",
                        endpoint_text(owner->host(), owner->tcp_port()).c_str());
            }
            go_idle();
        }
    }
};

NetPrinterDevice::NetPrinterDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames,
                                   const char *port_id, const std::string &host, int tcp_port)
    : SerialDevice("NetPrinterDevice", port_id), host_(host), tcp_port_(tcp_port),
      event_queue_(event_queue), frames_(frames), impl_(std::make_unique<Impl>()) {
    impl_->owner = this;
    if (frames_) {
        frame_handler_id_ = frames_->registerHandler([this]() {
            poll();
            return true;
        });
    }
    start_worker();
}

NetPrinterDevice::~NetPrinterDevice() {
    if (frames_ && frame_handler_id_ != static_cast<size_t>(-1)) {
        frames_->unregisterHandler(frame_handler_id_);
        frames_ = nullptr;
        frame_handler_id_ = static_cast<size_t>(-1);
    }
    if (thread) {
        SDL_Log("SerialDevice: %s shutting down", this->name);
        SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
        q_host.send(msg);
        SDL_WaitThread(thread, nullptr);
        thread = nullptr;
    }
    impl_.reset();
    event_queue_ = nullptr;
}

void NetPrinterDevice::device_loop() {
    while (true) {
        SDL_Delay(10);

        while (!q_host.is_empty()) {
            SerialMessage msg = q_host.get();
            switch (msg.type) {
                case MESSAGE_SHUTDOWN:
                    impl_->shutdown();
                    return;
                case MESSAGE_CLOSE:
                    impl_->go_idle();
                    break;
                case MESSAGE_DATA:
                    impl_->handle_data(static_cast<uint8_t>(msg.data), host_, tcp_port_);
                    break;
                default:
                    break;
            }
        }

        impl_->reap_resolve();
        impl_->poll_resolve(host_, tcp_port_);
        impl_->poll_connect(host_, tcp_port_);
        if (impl_->state == JobState::Open) {
            impl_->flush_out();
            impl_->drain_in(q_dev);
        }
        impl_->tick_idle();
    }
}

void NetPrinterDevice::format_hud_status(char *buf, size_t n) const {
    char handshake[64];
    format_handshake(handshake, sizeof(handshake));
    const char *up = socket_open_.load(std::memory_order_relaxed) ? "up" : "idle";
    if (host_.find(':') != std::string::npos) {
        std::snprintf(buf, n, "Net Printer [%s]:%d %s %s", host_.c_str(), tcp_port_, up, handshake);
    } else {
        std::snprintf(buf, n, "Net Printer %s:%d %s %s", host_.c_str(), tcp_port_, up, handshake);
    }
}

#endif
