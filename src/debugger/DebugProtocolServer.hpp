#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

struct computer_t;

/**
 * External debug protocol driver (AF_UNIX).
 * HELLO / PING / KEYEVENT on the protocol thread; GET_STATUS / RESET / READMEM / WRITEMEM via main-thread bridge.
 * See Docs/DebugProtocol.md.
 */
class DebugProtocolServer {
public:
    explicit DebugProtocolServer(std::string socket_path);
    ~DebugProtocolServer();

    DebugProtocolServer(const DebugProtocolServer&) = delete;
    DebugProtocolServer& operator=(const DebugProtocolServer&) = delete;

    bool start();
    void stop();

    /** Non-blocking. Call once per frame from the main / SDL iterate thread. */
    void process_main_thread(computer_t *computer);

    const std::string& socket_path() const { return socket_path_; }

private:
    static int SDLCALL thread_entry(void *userdata);
    void thread_main();
    void serve_client(int client_fd);
    bool read_full(int fd, void *buf, size_t n);
    bool write_full(int fd, const void *buf, size_t n);
    bool send_frame(int fd, uint32_t type, uint32_t seq, const void *payload, uint32_t length);
    bool send_error(int fd, uint32_t seq, uint32_t code, const char *message);

    /**
     * Submit a main-thread command and wait for reply.
     * On success, error_code_out==0 and reply holds the payload.
     * request_payload is copied into the bridge for commands that need inbound bytes (WRITEMEM).
     * Returns false if the server is stopping.
     */
    bool submit_and_wait(uint32_t type, uint32_t seq,
                         uint32_t arg0, uint32_t arg1, uint32_t arg2,
                         const std::vector<uint8_t> &request_payload,
                         std::vector<uint8_t> &reply_out, uint32_t &error_code_out,
                         int timeout_ms);

    void wake_bridge_locked();

    std::string socket_path_;
    std::atomic<bool> stop_{false};
    std::atomic<int> listen_fd_{-1};
    std::atomic<int> client_fd_{-1};
    SDL_Thread *thread_{nullptr};

    // Single-flight main-thread bridge
    std::mutex bridge_mu_;
    std::condition_variable bridge_cv_;
    bool bridge_pending_{false};
    bool bridge_done_{false};
    uint32_t bridge_type_{0};
    uint32_t bridge_seq_{0};
    uint32_t bridge_arg0_{0};
    uint32_t bridge_arg1_{0};
    uint32_t bridge_arg2_{0};
    uint32_t bridge_error_{0};
    bool bridge_timed_out_{false};
    std::vector<uint8_t> bridge_request_;
    std::vector<uint8_t> bridge_reply_;
};
