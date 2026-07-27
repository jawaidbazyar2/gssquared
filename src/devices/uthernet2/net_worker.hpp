#pragma once

#include "net_protocol.hpp"
#include "slirp_backend.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>

namespace u2 {

class NetWorker {
public:
    NetWorker();
    ~NetWorker();

    NetWorker(const NetWorker &) = delete;
    NetWorker &operator=(const NetWorker &) = delete;

    bool start();
    void stop();

    /** Non-blocking: post work for the worker thread. */
    bool post(NetRequest req);
    /** Non-blocking: drain one event if available. */
    bool poll_event(NetEvent *out);

    void wake();

private:
    static int thread_main(void *userdata);
    void run();
    void handle_request(const NetRequest &req);
    void poll_sockets();
    void post_event(NetEvent ev);

#ifdef _WIN32
    using socket_t = SOCKET;
    static constexpr socket_t kInvalid = INVALID_SOCKET;
#else
    using socket_t = int;
    static constexpr socket_t kInvalid = -1;
#endif

    struct SockState {
        socket_t fd = kInvalid;
        uint8_t status = W5100_SN_SR_CLOSED;
        bool is_tcp = false;
        bool is_udp = false;
        bool listening = false;
    };

    SDL_Thread *thread_ = nullptr;
    SDL_Semaphore *wake_ = nullptr;
    std::atomic<bool> running_{false};

    SpscRing<NetRequest, kRequestRingDepth> requests_;
    SpscRing<NetEvent, kEventRingDepth> events_;

    SockState socks_[U2_NUM_SOCKETS]{};
    SlirpBackend slirp_;
    bool slirp_ok_ = false;
};

}  // namespace u2
