/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "slirp_backend.hpp"

#include <libslirp.h>

#include <chrono>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace u2 {
namespace {

int64_t now_ns() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now().time_since_epoch()).count();
}

int64_t now_ms() {
    return now_ns() / 1000000;
}

}  // namespace

SlirpBackend::SlirpBackend() = default;

SlirpBackend::~SlirpBackend() {
    shutdown();
}

slirp_ssize_t SlirpBackend::send_packet_cb(const void *buf, size_t len, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    if (self->rx_cb_) {
        self->rx_cb_(static_cast<const uint8_t *>(buf), len);
    }
    return static_cast<slirp_ssize_t>(len);
}

void SlirpBackend::guest_error_cb(const char *msg, void * /*opaque*/) {
    fprintf(stderr, "Uthernet II/slirp guest error: %s\n", msg ? msg : "(null)");
}

int64_t SlirpBackend::clock_get_ns_cb(void * /*opaque*/) {
    return now_ns();
}

void *SlirpBackend::timer_new_cb(SlirpTimerCb cb, void *cb_opaque, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    auto *t = new Timer{cb, cb_opaque, -1};
    self->timers_.push_back(t);
    return t;
}

void SlirpBackend::timer_free_cb(void *timer, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    auto *t = static_cast<Timer *>(timer);
    for (auto it = self->timers_.begin(); it != self->timers_.end(); ++it) {
        if (*it == t) {
            self->timers_.erase(it);
            break;
        }
    }
    delete t;
}

void SlirpBackend::timer_mod_cb(void *timer, int64_t expire_time, void * /*opaque*/) {
    auto *t = static_cast<Timer *>(timer);
    t->expire_ms = expire_time;
}

void SlirpBackend::register_poll_fd_cb(int /*fd*/, void * /*opaque*/) {}
void SlirpBackend::unregister_poll_fd_cb(int /*fd*/, void * /*opaque*/) {}
void SlirpBackend::notify_cb(void * /*opaque*/) {}

void SlirpBackend::register_poll_socket_cb(slirp_os_socket socket, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    for (auto s : self->poll_sockets_) {
        if (s == socket) {
            return;
        }
    }
    self->poll_sockets_.push_back(socket);
}

void SlirpBackend::unregister_poll_socket_cb(slirp_os_socket socket, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    for (auto it = self->poll_sockets_.begin(); it != self->poll_sockets_.end(); ++it) {
        if (*it == socket) {
            self->poll_sockets_.erase(it);
            return;
        }
    }
}

int SlirpBackend::add_poll_cb(slirp_os_socket fd, int events, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    PollEntry e;
    e.fd = fd;
    e.events = events;
    e.revents = 0;
    self->poll_entries_.push_back(e);
    return static_cast<int>(self->poll_entries_.size() - 1);
}

int SlirpBackend::get_revents_cb(int idx, void *opaque) {
    auto *self = static_cast<SlirpBackend *>(opaque);
    if (idx < 0 || static_cast<size_t>(idx) >= self->poll_entries_.size()) {
        return 0;
    }
    return self->poll_entries_[static_cast<size_t>(idx)].revents;
}

bool SlirpBackend::init(const SlirpNetConfig &cfg) {
    shutdown();
    cfg_ = cfg;

    SlirpConfig sc{};
    memset(&sc, 0, sizeof(sc));
    sc.version = SLIRP_CONFIG_VERSION_MAX;
    sc.restricted = 0;
    sc.in_enabled = true;
    sc.vnetwork = cfg.vnetwork;
    sc.vnetmask = cfg.vnetmask;
    sc.vhost = cfg.vhost;
    sc.in6_enabled = false;
    sc.vdhcp_start = cfg.vdhcp_start;
    sc.vnameserver = cfg.vnameserver;
    sc.disable_dns = false;
    sc.disable_dhcp = false;

    memset(&cbs_, 0, sizeof(cbs_));
    cbs_.send_packet = send_packet_cb;
    cbs_.guest_error = guest_error_cb;
    cbs_.clock_get_ns = clock_get_ns_cb;
    cbs_.timer_new = timer_new_cb;
    cbs_.timer_free = timer_free_cb;
    cbs_.timer_mod = timer_mod_cb;
    cbs_.register_poll_fd = register_poll_fd_cb;
    cbs_.unregister_poll_fd = unregister_poll_fd_cb;
    cbs_.notify = notify_cb;
    cbs_.init_completed = nullptr;
    cbs_.timer_new_opaque = nullptr;
    cbs_.register_poll_socket = register_poll_socket_cb;
    cbs_.unregister_poll_socket = unregister_poll_socket_cb;

    slirp_ = slirp_new(&sc, &cbs_, this);
    if (!slirp_) {
        fprintf(stderr, "Uthernet II: slirp_new failed\n");
        return false;
    }
    return true;
}

void SlirpBackend::shutdown() {
    if (slirp_) {
        slirp_cleanup(slirp_);
        slirp_ = nullptr;
    }
    for (Timer *t : timers_) {
        delete t;
    }
    timers_.clear();
    poll_sockets_.clear();
    poll_entries_.clear();
}

void SlirpBackend::input(const uint8_t *pkt, size_t len) {
    if (slirp_ && pkt && len > 0) {
        slirp_input(slirp_, pkt, static_cast<int>(len));
    }
}

void SlirpBackend::poll(int timeout_ms) {
    if (!slirp_ || in_poll_) {
        return;
    }
    in_poll_ = true;

    // Fire expired timers
    const int64_t now = now_ms();
    for (Timer *t : timers_) {
        if (t && t->expire_ms >= 0 && t->expire_ms <= now) {
            t->expire_ms = -1;
            if (t->cb) {
                t->cb(t->opaque);
            }
        }
    }

    poll_entries_.clear();
    uint32_t timeout = timeout_ms < 0 ? UINT32_MAX : static_cast<uint32_t>(timeout_ms);
    slirp_pollfds_fill_socket(slirp_, &timeout, add_poll_cb, this);

    // Cap blocking: NetWorker already paces with a semaphore timeout.
    if (timeout_ms >= 0 && timeout > static_cast<uint32_t>(timeout_ms)) {
        timeout = static_cast<uint32_t>(timeout_ms);
    }

#ifdef _WIN32
    fd_set rfds, wfds, efds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    slirp_os_socket maxfd = 0;
    for (auto &e : poll_entries_) {
        if (e.events & SLIRP_POLL_IN) {
            FD_SET(e.fd, &rfds);
        }
        if (e.events & SLIRP_POLL_OUT) {
            FD_SET(e.fd, &wfds);
        }
        if (e.events & (SLIRP_POLL_ERR | SLIRP_POLL_HUP)) {
            FD_SET(e.fd, &efds);
        }
        if (e.fd > maxfd) {
            maxfd = e.fd;
        }
    }
    timeval tv{};
    timeval *ptv = nullptr;
    if (timeout != UINT32_MAX) {
        tv.tv_sec = timeout / 1000;
        tv.tv_usec = (timeout % 1000) * 1000;
        ptv = &tv;
    }
    const int sel = select(static_cast<int>(maxfd + 1), &rfds, &wfds, &efds, ptv);
    if (sel > 0) {
        for (auto &e : poll_entries_) {
            if (FD_ISSET(e.fd, &rfds)) {
                e.revents |= SLIRP_POLL_IN;
            }
            if (FD_ISSET(e.fd, &wfds)) {
                e.revents |= SLIRP_POLL_OUT;
            }
            if (FD_ISSET(e.fd, &efds)) {
                e.revents |= SLIRP_POLL_ERR;
            }
        }
    }
    slirp_pollfds_poll(slirp_, sel < 0, get_revents_cb, this);
#else
    std::vector<pollfd> pfds(poll_entries_.size());
    for (size_t i = 0; i < poll_entries_.size(); ++i) {
        pfds[i].fd = poll_entries_[i].fd;
        pfds[i].events = 0;
        pfds[i].revents = 0;
        if (poll_entries_[i].events & SLIRP_POLL_IN) {
            pfds[i].events |= POLLIN;
        }
        if (poll_entries_[i].events & SLIRP_POLL_OUT) {
            pfds[i].events |= POLLOUT;
        }
        if (poll_entries_[i].events & SLIRP_POLL_PRI) {
            pfds[i].events |= POLLPRI;
        }
    }
    const int to = (timeout == UINT32_MAX) ? -1 : static_cast<int>(timeout);
    const int pr = ::poll(pfds.empty() ? nullptr : pfds.data(),
                          static_cast<nfds_t>(pfds.size()), to);
    if (pr > 0) {
        for (size_t i = 0; i < pfds.size(); ++i) {
            int rev = 0;
            if (pfds[i].revents & POLLIN) {
                rev |= SLIRP_POLL_IN;
            }
            if (pfds[i].revents & POLLOUT) {
                rev |= SLIRP_POLL_OUT;
            }
            if (pfds[i].revents & POLLPRI) {
                rev |= SLIRP_POLL_PRI;
            }
            if (pfds[i].revents & POLLERR) {
                rev |= SLIRP_POLL_ERR;
            }
            if (pfds[i].revents & POLLHUP) {
                rev |= SLIRP_POLL_HUP;
            }
            poll_entries_[i].revents = rev;
        }
    }
    slirp_pollfds_poll(slirp_, pr < 0, get_revents_cb, this);
#endif
    in_poll_ = false;
}

}  // namespace u2
