#include "debugger/DebugProtocolServer.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <SDL3/SDL.h>

#include "computer.hpp"
#include "cpu.hpp"
#include "mmus/mmu.hpp"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#define GS2_DEBUG_PROTO_UNIX 1
#else
#define GS2_DEBUG_PROTO_UNIX 0
#endif

namespace {

constexpr uint32_t kProtoVersion = 1;
constexpr uint32_t kMaxPayload = 0x00100000;
constexpr uint32_t kMaxReadMem = 65536;
constexpr uint32_t kMaxWriteMem = 65536;

constexpr uint32_t kTypeHello     = 0x00000001;
constexpr uint32_t kTypePing      = 0x00000002;
constexpr uint32_t kTypeError     = 0x00000003;
constexpr uint32_t kTypeGetStatus = 0x00000101;
constexpr uint32_t kTypeReset     = 0x00000102;
constexpr uint32_t kTypeReadMem   = 0x00000301;
constexpr uint32_t kTypeWriteMem  = 0x00000302;
constexpr uint32_t kTypeKeyEvent  = 0x00000501;

constexpr uint32_t kMemMain    = 0;
constexpr uint32_t kMemMegaII  = 1;
constexpr uint32_t kMemEnsoniq = 2;
constexpr uint32_t kMemAdbMicro = 3;

constexpr uint32_t kEUnknownType   = 1;
constexpr uint32_t kEBadLength     = 2;
constexpr uint32_t kEBadVersion    = 3;
constexpr uint32_t kENotHandshaked = 4;
constexpr uint32_t kEBusy          = 5;
constexpr uint32_t kEInternal      = 6;

constexpr int kMainThreadTimeoutMs = 5000;

#pragma pack(push, 1)
struct FrameHeader {
    uint32_t type;
    uint32_t seq;
    uint32_t length;
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == 12, "FrameHeader must be 12 bytes");

} // namespace

DebugProtocolServer::DebugProtocolServer(std::string socket_path)
    : socket_path_(std::move(socket_path)) {}

DebugProtocolServer::~DebugProtocolServer() {
    stop();
}

bool DebugProtocolServer::start() {
#if !GS2_DEBUG_PROTO_UNIX
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "DebugProtocolServer: Unix domain sockets not supported on this platform");
    return false;
#else
    if (thread_) {
        return true;
    }
    if (socket_path_.empty()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: empty socket path");
        return false;
    }

    stop_ = false;
    int lfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (lfd < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: socket(): %s", strerror(errno));
        return false;
    }

    ::unlink(socket_path_.c_str());

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: path too long: %s", socket_path_.c_str());
        ::close(lfd);
        return false;
    }
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());

    if (::bind(lfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: bind(%s): %s",
                     socket_path_.c_str(), strerror(errno));
        ::close(lfd);
        return false;
    }

    if (::listen(lfd, 1) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: listen(): %s", strerror(errno));
        ::close(lfd);
        ::unlink(socket_path_.c_str());
        return false;
    }

    listen_fd_ = lfd;
    thread_ = SDL_CreateThread(thread_entry, "gs2-debug-proto", this);
    if (!thread_) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: SDL_CreateThread failed: %s",
                     SDL_GetError());
        listen_fd_ = -1;
        ::close(lfd);
        ::unlink(socket_path_.c_str());
        return false;
    }

    SDL_Log("DebugProtocolServer: listening on %s", socket_path_.c_str());
    return true;
#endif
}

void DebugProtocolServer::stop() {
    stop_ = true;
    {
        std::lock_guard<std::mutex> lock(bridge_mu_);
        wake_bridge_locked();
    }
#if GS2_DEBUG_PROTO_UNIX
    int cfd = client_fd_.exchange(-1);
    if (cfd >= 0) {
        ::shutdown(cfd, SHUT_RDWR);
    }
    int lfd = listen_fd_.exchange(-1);
    if (lfd >= 0) {
        ::shutdown(lfd, SHUT_RDWR);
        ::close(lfd);
    }
#endif
    if (thread_) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
#if GS2_DEBUG_PROTO_UNIX
    if (!socket_path_.empty()) {
        ::unlink(socket_path_.c_str());
    }
#endif
}

void DebugProtocolServer::wake_bridge_locked() {
    if (bridge_pending_ && !bridge_done_) {
        bridge_error_ = kEInternal;
        bridge_done_ = true;
        bridge_cv_.notify_one();
    }
}

void DebugProtocolServer::process_main_thread(computer_t *computer) {
    std::lock_guard<std::mutex> lock(bridge_mu_);
    if (!bridge_pending_ || bridge_done_) {
        return;
    }

    bridge_reply_.clear();
    bridge_error_ = 0;
    bridge_timed_out_ = false;

    if (bridge_type_ == kTypeGetStatus) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            uint32_t mode = static_cast<uint32_t>(computer->execution_mode);
            uint32_t platform_id = computer->platform
                ? static_cast<uint32_t>(computer->platform->id)
                : 0xFFFFFFFFu;
            bridge_reply_.resize(8);
            std::memcpy(bridge_reply_.data() + 0, &mode, 4);
            std::memcpy(bridge_reply_.data() + 4, &platform_id, 4);
        }
    } else if (bridge_type_ == kTypeReset) {
        const uint32_t cold_start = bridge_arg0_;
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            computer->reset(cold_start != 0);
        }
    } else if (bridge_type_ == kTypeReadMem) {
        const uint32_t domain = bridge_arg0_;
        const uint32_t address = bridge_arg1_;
        const uint32_t length = bridge_arg2_;

        if (domain != kMemMain) {
            bridge_error_ = kEInternal;
            // Message chosen in serve_client from error code + type
        } else if (!computer || !computer->cpu || !computer->cpu->mmu) {
            bridge_error_ = kEInternal;
        } else {
            MMU *mmu = computer->cpu->mmu;
            bridge_reply_.resize(length);
            for (uint32_t i = 0; i < length; ++i) {
                bridge_reply_[i] = mmu->read(address + i);
            }
        }
    } else if (bridge_type_ == kTypeWriteMem) {
        const uint32_t domain = bridge_arg0_;
        const uint32_t address = bridge_arg1_;
        const uint32_t length = bridge_arg2_;

        if (domain != kMemMain) {
            bridge_error_ = kEInternal;
        } else if (!computer || !computer->cpu || !computer->cpu->mmu) {
            bridge_error_ = kEInternal;
        } else if (bridge_request_.size() != length) {
            bridge_error_ = kEInternal;
        } else {
            MMU *mmu = computer->cpu->mmu;
            for (uint32_t i = 0; i < length; ++i) {
                mmu->write(address + i, bridge_request_[i]);
            }
        }
    } else {
        bridge_error_ = kEInternal;
    }

    bridge_done_ = true;
    bridge_cv_.notify_one();
}

bool DebugProtocolServer::submit_and_wait(uint32_t type, uint32_t seq,
                                          uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                          const std::vector<uint8_t> &request_payload,
                                          std::vector<uint8_t> &reply_out, uint32_t &error_code_out,
                                          int timeout_ms) {
    std::unique_lock<std::mutex> lock(bridge_mu_);
    if (bridge_pending_) {
        error_code_out = kEBusy;
        reply_out.clear();
        return true;
    }

    bridge_pending_ = true;
    bridge_done_ = false;
    bridge_timed_out_ = false;
    bridge_type_ = type;
    bridge_seq_ = seq;
    bridge_arg0_ = arg0;
    bridge_arg1_ = arg1;
    bridge_arg2_ = arg2;
    bridge_error_ = 0;
    bridge_request_ = request_payload;
    bridge_reply_.clear();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (!bridge_done_ && !stop_) {
        if (bridge_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            bridge_pending_ = false;
            bridge_done_ = false;
            error_code_out = kEInternal;
            bridge_timed_out_ = true;
            reply_out.clear();
            return true;
        }
    }

    if (stop_ && !bridge_done_) {
        bridge_pending_ = false;
        return false;
    }

    error_code_out = bridge_error_;
    reply_out = std::move(bridge_reply_);
    bridge_request_.clear();
    bridge_pending_ = false;
    bridge_done_ = false;
    return true;
}

int SDLCALL DebugProtocolServer::thread_entry(void *userdata) {
    static_cast<DebugProtocolServer *>(userdata)->thread_main();
    return 0;
}

void DebugProtocolServer::thread_main() {
#if GS2_DEBUG_PROTO_UNIX
    const int lfd = listen_fd_.load();
    while (!stop_ && lfd >= 0) {
        int client_fd = ::accept(lfd, nullptr, nullptr);
        if (client_fd < 0) {
            if (stop_ || errno != EINTR) {
                break;
            }
            continue;
        }

        if (stop_) {
            ::close(client_fd);
            break;
        }

        client_fd_ = client_fd;
        SDL_Log("DebugProtocolServer: client connected");
        serve_client(client_fd);
        client_fd_ = -1;
        ::close(client_fd);
        SDL_Log("DebugProtocolServer: client disconnected");
    }
#endif
}

bool DebugProtocolServer::read_full(int fd, void *buf, size_t n) {
#if GS2_DEBUG_PROTO_UNIX
    auto *p = static_cast<uint8_t *>(buf);
    size_t got = 0;
    while (got < n) {
        if (stop_) {
            return false;
        }
        ssize_t r = ::recv(fd, p + got, n - got, 0);
        if (r == 0) {
            return false;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        got += static_cast<size_t>(r);
    }
    return true;
#else
    (void)fd;
    (void)buf;
    (void)n;
    return false;
#endif
}

bool DebugProtocolServer::write_full(int fd, const void *buf, size_t n) {
#if GS2_DEBUG_PROTO_UNIX
    auto *p = static_cast<const uint8_t *>(buf);
    size_t sent = 0;
    while (sent < n) {
        if (stop_) {
            return false;
        }
        ssize_t w = ::send(fd, p + sent, n - sent, 0);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        sent += static_cast<size_t>(w);
    }
    return true;
#else
    (void)fd;
    (void)buf;
    (void)n;
    return false;
#endif
}

bool DebugProtocolServer::send_frame(int fd, uint32_t type, uint32_t seq, const void *payload, uint32_t length) {
    FrameHeader hdr{type, seq, length};
    if (!write_full(fd, &hdr, sizeof(hdr))) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    return write_full(fd, payload, length);
}

bool DebugProtocolServer::send_error(int fd, uint32_t seq, uint32_t code, const char *message) {
    const size_t msg_len = message ? std::strlen(message) : 0;
    std::vector<uint8_t> payload(4 + msg_len);
    std::memcpy(payload.data(), &code, 4);
    if (msg_len) {
        std::memcpy(payload.data() + 4, message, msg_len);
    }
    return send_frame(fd, kTypeError, seq, payload.data(), static_cast<uint32_t>(payload.size()));
}

void DebugProtocolServer::serve_client(int client_fd) {
    bool handshaked = false;

    while (!stop_) {
        FrameHeader hdr{};
        if (!read_full(client_fd, &hdr, sizeof(hdr))) {
            return;
        }

        if (hdr.length > kMaxPayload) {
            send_error(client_fd, hdr.seq, kEBadLength, "payload too large");
            return;
        }

        std::vector<uint8_t> payload(hdr.length);
        if (hdr.length > 0) {
            if (!read_full(client_fd, payload.data(), hdr.length)) {
                return;
            }
        }

        if ((hdr.type & 0xFF000000u) != 0) {
            if (!send_error(client_fd, hdr.seq, kEUnknownType, "flags must be zero on requests")) {
                return;
            }
            continue;
        }

        switch (hdr.type) {
        case kTypeHello: {
            if (hdr.length != 8) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "HELLO requires 8-byte payload")) {
                    return;
                }
                continue;
            }
            uint32_t client_version = 0;
            std::memcpy(&client_version, payload.data(), 4);
            if (client_version != kProtoVersion) {
                if (!send_error(client_fd, hdr.seq, kEBadVersion, "unsupported protocol version")) {
                    return;
                }
                continue;
            }
            uint8_t reply[12];
            uint32_t version = kProtoVersion;
            uint32_t flags = 0;
            uint32_t max_payload = kMaxPayload;
            std::memcpy(reply + 0, &version, 4);
            std::memcpy(reply + 4, &flags, 4);
            std::memcpy(reply + 8, &max_payload, 4);
            if (!send_frame(client_fd, kTypeHello, hdr.seq, reply, 12)) {
                return;
            }
            handshaked = true;
            break;
        }
        case kTypePing: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 0) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "PING requires empty payload")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypePing, hdr.seq, nullptr, 0)) {
                return;
            }
            break;
        }
        case kTypeGetStatus: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 0) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "GET_STATUS requires empty payload")) {
                    return;
                }
                continue;
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeGetStatus, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                const char *msg = "internal error";
                if (err == kEBusy) {
                    msg = "busy";
                } else if (err == kEInternal) {
                    std::lock_guard<std::mutex> lock(bridge_mu_);
                    msg = bridge_timed_out_ ? "timeout waiting for main thread" : "no machine";
                }
                if (!send_error(client_fd, hdr.seq, err, msg)) {
                    return;
                }
                continue;
            }
            if (reply.size() != 8) {
                if (!send_error(client_fd, hdr.seq, kEInternal, "bad status reply")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypeGetStatus, hdr.seq, reply.data(), 8)) {
                return;
            }
            break;
        }
        case kTypeReset: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 4) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "RESET requires 4-byte payload")) {
                    return;
                }
                continue;
            }
            uint32_t cold_start = 0;
            std::memcpy(&cold_start, payload.data(), 4);
            if (cold_start != 0 && cold_start != 1) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "RESET cold_start must be 0 or 1")) {
                    return;
                }
                continue;
            }

            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeReset, hdr.seq, cold_start, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                const char *msg = "internal error";
                if (err == kEBusy) {
                    msg = "busy";
                } else if (err == kEInternal) {
                    std::lock_guard<std::mutex> lock(bridge_mu_);
                    msg = bridge_timed_out_ ? "timeout waiting for main thread" : "no machine";
                }
                if (!send_error(client_fd, hdr.seq, err, msg)) {
                    return;
                }
                continue;
            }
            if (!reply.empty()) {
                if (!send_error(client_fd, hdr.seq, kEInternal, "bad reset reply")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypeReset, hdr.seq, nullptr, 0)) {
                return;
            }
            break;
        }
        case kTypeReadMem: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 12) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "READMEM requires 12-byte payload")) {
                    return;
                }
                continue;
            }
            uint32_t domain = 0, address = 0, length = 0;
            std::memcpy(&domain, payload.data() + 0, 4);
            std::memcpy(&address, payload.data() + 4, 4);
            std::memcpy(&length, payload.data() + 8, 4);

            if (length == 0 || length > kMaxReadMem) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "READMEM length out of range")) {
                    return;
                }
                continue;
            }
            if (address > std::numeric_limits<uint32_t>::max() - length) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "READMEM address wrap")) {
                    return;
                }
                continue;
            }
            if (domain != kMemMain && domain != kMemMegaII && domain != kMemEnsoniq && domain != kMemAdbMicro) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "READMEM invalid domain")) {
                    return;
                }
                continue;
            }

            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeReadMem, hdr.seq, domain, address, length, kEmptyRequest, reply,
                                 err, kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                const char *msg = "internal error";
                if (err == kEBusy) {
                    msg = "busy";
                } else if (err == kEInternal) {
                    std::lock_guard<std::mutex> lock(bridge_mu_);
                    if (bridge_timed_out_) {
                        msg = "timeout waiting for main thread";
                    } else if (domain != kMemMain) {
                        msg = "unsupported domain";
                    } else {
                        msg = "no machine";
                    }
                }
                if (!send_error(client_fd, hdr.seq, err, msg)) {
                    return;
                }
                continue;
            }
            if (reply.size() != length) {
                if (!send_error(client_fd, hdr.seq, kEInternal, "bad readmem reply")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypeReadMem, hdr.seq, reply.data(), length)) {
                return;
            }
            break;
        }
        case kTypeWriteMem: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length < 12) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "WRITEMEM payload too short")) {
                    return;
                }
                continue;
            }
            uint32_t domain = 0, address = 0, length = 0;
            std::memcpy(&domain, payload.data() + 0, 4);
            std::memcpy(&address, payload.data() + 4, 4);
            std::memcpy(&length, payload.data() + 8, 4);

            if (length == 0 || length > kMaxWriteMem) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "WRITEMEM length out of range")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 12 + length) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "WRITEMEM payload length mismatch")) {
                    return;
                }
                continue;
            }
            if (address > std::numeric_limits<uint32_t>::max() - length) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "WRITEMEM address wrap")) {
                    return;
                }
                continue;
            }
            if (domain != kMemMain && domain != kMemMegaII && domain != kMemEnsoniq && domain != kMemAdbMicro) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "WRITEMEM invalid domain")) {
                    return;
                }
                continue;
            }

            std::vector<uint8_t> request_data(payload.begin() + 12, payload.end());
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeWriteMem, hdr.seq, domain, address, length, request_data, reply,
                                 err, kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                const char *msg = "internal error";
                if (err == kEBusy) {
                    msg = "busy";
                } else if (err == kEInternal) {
                    std::lock_guard<std::mutex> lock(bridge_mu_);
                    if (bridge_timed_out_) {
                        msg = "timeout waiting for main thread";
                    } else if (domain != kMemMain) {
                        msg = "unsupported domain";
                    } else {
                        msg = "no machine";
                    }
                }
                if (!send_error(client_fd, hdr.seq, err, msg)) {
                    return;
                }
                continue;
            }
            if (!reply.empty()) {
                if (!send_error(client_fd, hdr.seq, kEInternal, "bad writemem reply")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypeWriteMem, hdr.seq, nullptr, 0)) {
                return;
            }
            break;
        }
        case kTypeKeyEvent: {
            if (!handshaked) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (hdr.length != 12) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "KEYEVENT requires 12-byte payload")) {
                    return;
                }
                continue;
            }
            uint32_t down = 0, scancode = 0, mod = 0;
            std::memcpy(&down, payload.data() + 0, 4);
            std::memcpy(&scancode, payload.data() + 4, 4);
            std::memcpy(&mod, payload.data() + 8, 4);

            if (down != 0 && down != 1) {
                if (!send_error(client_fd, hdr.seq, kEBadLength, "KEYEVENT down must be 0 or 1")) {
                    return;
                }
                continue;
            }

            SDL_Event ev{};
            ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            ev.key.scancode = static_cast<SDL_Scancode>(scancode);
            ev.key.mod = static_cast<SDL_Keymod>(mod);
            ev.key.key = SDL_GetKeyFromScancode(ev.key.scancode, ev.key.mod, false);
            ev.key.down = (down != 0);
            ev.key.repeat = false;

            if (!SDL_PushEvent(&ev)) {
                if (!send_error(client_fd, hdr.seq, kEInternal, "SDL_PushEvent failed")) {
                    return;
                }
                continue;
            }
            if (!send_frame(client_fd, kTypeKeyEvent, hdr.seq, nullptr, 0)) {
                return;
            }
            break;
        }
        default: {
            if (!handshaked && hdr.type != kTypeHello) {
                if (!send_error(client_fd, hdr.seq, kENotHandshaked, "HELLO required first")) {
                    return;
                }
                continue;
            }
            if (!send_error(client_fd, hdr.seq, kEUnknownType, "unknown type")) {
                return;
            }
            break;
        }
        }
    }
}
