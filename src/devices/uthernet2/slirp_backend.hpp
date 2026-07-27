#pragma once

#include "net_protocol.hpp"
#include "slirp_netpick.hpp"

#include <libslirp.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace u2 {

/** libslirp wrapper owned by the network worker thread. */
class SlirpBackend {
public:
    using RxCallback = std::function<void(const uint8_t *pkt, size_t len)>;

    SlirpBackend();
    ~SlirpBackend();

    SlirpBackend(const SlirpBackend &) = delete;
    SlirpBackend &operator=(const SlirpBackend &) = delete;

    bool init(const SlirpNetConfig &cfg);
    void shutdown();

    void input(const uint8_t *pkt, size_t len);
    void poll(int timeout_ms);

    void set_rx_callback(RxCallback cb) { rx_cb_ = std::move(cb); }

    const SlirpNetConfig &config() const { return cfg_; }

private:
    static slirp_ssize_t send_packet_cb(const void *buf, size_t len, void *opaque);
    static void guest_error_cb(const char *msg, void *opaque);
    static int64_t clock_get_ns_cb(void *opaque);
    static void *timer_new_cb(SlirpTimerCb cb, void *cb_opaque, void *opaque);
    static void timer_free_cb(void *timer, void *opaque);
    static void timer_mod_cb(void *timer, int64_t expire_time, void *opaque);
    static void register_poll_fd_cb(int fd, void *opaque);
    static void unregister_poll_fd_cb(int fd, void *opaque);
    static void notify_cb(void *opaque);
    static void register_poll_socket_cb(slirp_os_socket socket, void *opaque);
    static void unregister_poll_socket_cb(slirp_os_socket socket, void *opaque);
    static int add_poll_cb(slirp_os_socket fd, int events, void *opaque);
    static int get_revents_cb(int idx, void *opaque);

    struct Timer {
        SlirpTimerCb cb = nullptr;
        void *opaque = nullptr;
        int64_t expire_ms = -1;
    };

    Slirp *slirp_ = nullptr;
    SlirpNetConfig cfg_{};
    RxCallback rx_cb_;
    std::vector<Timer *> timers_;
    std::vector<slirp_os_socket> poll_sockets_;

    struct PollEntry {
        slirp_os_socket fd{};
        int events = 0;
        int revents = 0;
    };
    std::vector<PollEntry> poll_entries_;

    // libslirp keeps a pointer to SlirpCb for the lifetime of the Slirp*;
    // it must outlive this instance (not a stack temporary).
    SlirpCb cbs_{};
    bool in_poll_ = false;
};

}  // namespace u2
