#include "debugger/DebugProtocolServer.hpp"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <vector>

#include <SDL3/SDL.h>

#include "computer.hpp"
#include "cpu.hpp"
#include "gs2.hpp"
#include "mmus/mmu.hpp"
#include "PlatformIDs.hpp"

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <poll.h>
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
constexpr uint32_t kTypeEvent     = 0x00000004;
constexpr uint32_t kTypeQuit      = 0x00000005;
constexpr uint32_t kTypeGetStatus = 0x00000101;
constexpr uint32_t kTypeReset     = 0x00000102;
constexpr uint32_t kTypePause     = 0x00000103;
constexpr uint32_t kTypeContinue  = 0x00000104;
constexpr uint32_t kTypeStepInto  = 0x00000105;
constexpr uint32_t kTypeReadMem   = 0x00000301;
constexpr uint32_t kTypeWriteMem  = 0x00000302;
constexpr uint32_t kTypeBpSet     = 0x00000401;
constexpr uint32_t kTypeBpClear   = 0x00000402;
constexpr uint32_t kTypeBpClearAll = 0x00000403;
constexpr uint32_t kTypeBpEnable  = 0x00000404;
constexpr uint32_t kTypeBpList    = 0x00000405;
constexpr uint32_t kTypeKeyEvent  = 0x00000501;

constexpr uint32_t kEvtStopped   = 1;
constexpr uint32_t kEvtRunState  = 2;

constexpr uint32_t kBpSetPayloadSize = 32;
constexpr uint32_t kBpListRecordSize = 40;

constexpr uint32_t kMemMain    = 0;
constexpr uint32_t kMemMegaII  = 1;
constexpr uint32_t kMemEnsoniq = 2;
constexpr uint32_t kMemAdbMicro = 3;
constexpr uint32_t kMemMainRaw = 4;
constexpr uint32_t kMemMegaIIRaw = 5;

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
static_assert(sizeof(system_trace_entry_t) == 40, "system_trace_entry_t wire size must be 40");

/** Last stop reason for Policy A on CONTINUE (single server instance). */
uint32_t g_last_stop_reason = 0;

void pack_bp_fields(uint8_t *out32, const bp_entry_t &e) {
    out32[0] = e.kind;
    out32[1] = e.flags;
    out32[2] = e.access;
    out32[3] = e.pad;
    std::memcpy(out32 + 4, &e.domain, 4);
    std::memcpy(out32 + 8, &e.address, 4);
    std::memcpy(out32 + 12, &e.length, 4);
    std::memcpy(out32 + 16, &e.addr_mask, 4);
    std::memcpy(out32 + 20, &e.data_value, 4);
    std::memcpy(out32 + 24, &e.data_mask, 4);
    std::memcpy(out32 + 28, &e.ignore_count, 4);
}

bool parse_bp_set_request(const std::vector<uint8_t> &payload, bp_entry_t &out) {
    if (payload.size() != kBpSetPayloadSize) {
        return false;
    }
    out = {};
    out.kind = payload[0];
    out.flags = payload[1];
    out.access = payload[2];
    out.pad = payload[3];
    std::memcpy(&out.domain, payload.data() + 4, 4);
    std::memcpy(&out.address, payload.data() + 8, 4);
    std::memcpy(&out.length, payload.data() + 12, 4);
    std::memcpy(&out.addr_mask, payload.data() + 16, 4);
    std::memcpy(&out.data_value, payload.data() + 20, 4);
    std::memcpy(&out.data_mask, payload.data() + 24, 4);
    std::memcpy(&out.ignore_count, payload.data() + 28, 4);
    if (out.addr_mask == 0) {
        out.addr_mask = 0xFFFFFFFFu;
    }
    return true;
}

uint32_t map_bp_add_error(const char *msg) {
    if (!msg) {
        return kEInternal;
    }
    if (std::strcmp(msg, "too many breakpoints") == 0
        || std::strcmp(msg, "length == 0") == 0
        || std::strcmp(msg, "out of range") == 0
        || std::strcmp(msg, "bad kind") == 0
        || std::strcmp(msg, "bad flags") == 0
        || std::strcmp(msg, "bad access") == 0) {
        return kEBadLength;
    }
    return kEInternal;
}

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
    bridge_error_text_.clear();
    bridge_timed_out_ = false;
    bridge_megaii_platform_reject_ = false;

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
    } else if (bridge_type_ == kTypeQuit) {
        // Force-quit: skip QuitModal / dirty-disk prompts, halt, exit process.
        // Do not SDL_PushEvent(QUIT) — that races with AppQuit when the debug
        // thread is still finishing the QUIT reply.
        gs2_app_values.no_quit_confirm = true;
        gs2_app_values.force_app_exit = true;
        if (computer && computer->cpu) {
            computer->cpu->halt = HLT_USER;
        }
    } else if (bridge_type_ == kTypeReadMem) {
        const uint32_t domain = bridge_arg0_;
        const uint32_t address = bridge_arg1_;
        const uint32_t length = bridge_arg2_;

        if (domain == kMemMain) {
            if (!computer || !computer->cpu || !computer->cpu->mmu) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->cpu->mmu;
                bridge_reply_.resize(length);
                for (uint32_t i = 0; i < length; ++i) {
                    bridge_reply_[i] = mmu->read(address + i);
                }
            }
        } else if (domain == kMemMegaII) {
            if (!computer || !computer->platform
                || computer->platform->id != PLATFORM_APPLE_IIGS) {
                bridge_error_ = kEInternal;
                bridge_megaii_platform_reject_ = true;
            } else if (!computer->mmu) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->mmu;
                bridge_reply_.resize(length);
                for (uint32_t i = 0; i < length; ++i) {
                    bridge_reply_[i] = mmu->read(address + i);
                }
            }
        } else if (domain == kMemMainRaw) {
            if (!computer || !computer->cpu || !computer->cpu->mmu) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->cpu->mmu;
                uint8_t *base = mmu->get_memory_base();
                uint32_t size = mmu->get_memory_size();
                if (!base || size == 0) {
                    bridge_error_ = kEInternal;
                } else if (address > size || length > size - address) {
                    bridge_error_ = kEBadLength;
                } else {
                    bridge_reply_.assign(base + address, base + address + length);
                }
            }
        } else if (domain == kMemMegaIIRaw) {
            if (!computer || !computer->platform
                || computer->platform->id != PLATFORM_APPLE_IIGS) {
                bridge_error_ = kEInternal;
                bridge_megaii_platform_reject_ = true;
            } else if (!computer->mmu) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->mmu;
                uint8_t *base = mmu->get_memory_base();
                uint32_t size = mmu->get_memory_size();
                if (!base || size == 0) {
                    bridge_error_ = kEInternal;
                } else if (address > size || length > size - address) {
                    bridge_error_ = kEBadLength;
                } else {
                    bridge_reply_.assign(base + address, base + address + length);
                }
            }
        } else {
            bridge_error_ = kEInternal;
        }
    } else if (bridge_type_ == kTypeWriteMem) {
        const uint32_t domain = bridge_arg0_;
        const uint32_t address = bridge_arg1_;
        const uint32_t length = bridge_arg2_;

        if (domain == kMemMain) {
            if (!computer || !computer->cpu || !computer->cpu->mmu) {
                bridge_error_ = kEInternal;
            } else if (bridge_request_.size() != length) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->cpu->mmu;
                for (uint32_t i = 0; i < length; ++i) {
                    mmu->write(address + i, bridge_request_[i]);
                }
            }
        } else if (domain == kMemMegaII) {
            if (!computer || !computer->platform
                || computer->platform->id != PLATFORM_APPLE_IIGS) {
                bridge_error_ = kEInternal;
                bridge_megaii_platform_reject_ = true;
            } else if (!computer->mmu) {
                bridge_error_ = kEInternal;
            } else if (bridge_request_.size() != length) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->mmu;
                for (uint32_t i = 0; i < length; ++i) {
                    mmu->write(address + i, bridge_request_[i]);
                }
            }
        } else if (domain == kMemMainRaw) {
            if (!computer || !computer->cpu || !computer->cpu->mmu) {
                bridge_error_ = kEInternal;
            } else if (bridge_request_.size() != length) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->cpu->mmu;
                uint8_t *base = mmu->get_memory_base();
                uint32_t size = mmu->get_memory_size();
                if (!base || size == 0) {
                    bridge_error_ = kEInternal;
                } else if (address > size || length > size - address) {
                    bridge_error_ = kEBadLength;
                } else {
                    std::memcpy(base + address, bridge_request_.data(), length);
                }
            }
        } else if (domain == kMemMegaIIRaw) {
            if (!computer || !computer->platform
                || computer->platform->id != PLATFORM_APPLE_IIGS) {
                bridge_error_ = kEInternal;
                bridge_megaii_platform_reject_ = true;
            } else if (!computer->mmu) {
                bridge_error_ = kEInternal;
            } else if (bridge_request_.size() != length) {
                bridge_error_ = kEInternal;
            } else {
                MMU *mmu = computer->mmu;
                uint8_t *base = mmu->get_memory_base();
                uint32_t size = mmu->get_memory_size();
                if (!base || size == 0) {
                    bridge_error_ = kEInternal;
                } else if (address > size || length > size - address) {
                    bridge_error_ = kEBadLength;
                } else {
                    std::memcpy(base + address, bridge_request_.data(), length);
                }
            }
        } else {
            bridge_error_ = kEInternal;
        }
    } else if (bridge_type_ == kTypePause) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            const execution_modes_t prev = computer->execution_mode;
            computer->execution_mode = EXEC_PAUSED;
            g_last_stop_reason = STOP_PAUSE;
            emit_stopped_pause(computer);
            emit_run_state(static_cast<uint32_t>(EXEC_PAUSED), static_cast<uint32_t>(prev));
        }
    } else if (bridge_type_ == kTypeContinue) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            const execution_modes_t prev = computer->execution_mode;
            computer->execution_mode = EXEC_NORMAL;
            if (computer->breakpoints && computer->cpu) {
                if (prev == EXEC_STEP_INTO || g_last_stop_reason == STOP_BP_EXEC) {
                    computer->breakpoints->arm_exec_suppress(computer->cpu->full_pc);
                }
            }
            g_last_stop_reason = 0;
            emit_run_state(static_cast<uint32_t>(EXEC_NORMAL), static_cast<uint32_t>(prev));
        }
    } else if (bridge_type_ == kTypeStepInto) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else if (bridge_arg0_ == 0) {
            bridge_error_ = kEBadLength;
            bridge_error_text_ = "STEP_INTO count must be >= 1";
        } else {
            const execution_modes_t prev = computer->execution_mode;
            computer->execution_mode = EXEC_STEP_INTO;
            computer->instructions_left = bridge_arg0_;
            g_last_stop_reason = 0;
            emit_run_state(static_cast<uint32_t>(EXEC_STEP_INTO), static_cast<uint32_t>(prev));
        }
    } else if (bridge_type_ == kTypeBpSet) {
        if (!computer || !computer->breakpoints) {
            bridge_error_ = kEInternal;
        } else if (bridge_request_.size() != kBpSetPayloadSize) {
            bridge_error_ = kEBadLength;
        } else {
            bp_entry_t req{};
            if (!parse_bp_set_request(bridge_request_, req)) {
                bridge_error_ = kEBadLength;
            } else {
                const char *add_err = nullptr;
                const uint32_t id = computer->breakpoints->add(req, &add_err);
                if (id == 0) {
                    bridge_error_ = map_bp_add_error(add_err);
                    if (add_err) {
                        bridge_error_text_ = add_err;
                    }
                } else {
                    bridge_reply_.resize(4);
                    std::memcpy(bridge_reply_.data(), &id, 4);
                }
            }
        }
    } else if (bridge_type_ == kTypeBpClear) {
        if (!computer || !computer->breakpoints) {
            bridge_error_ = kEInternal;
        } else if (!computer->breakpoints->clear_id(bridge_arg0_)) {
            bridge_error_ = kEInternal;
            bridge_error_text_ = "unknown id";
        }
    } else if (bridge_type_ == kTypeBpClearAll) {
        if (!computer || !computer->breakpoints) {
            bridge_error_ = kEInternal;
        } else {
            computer->breakpoints->clear_all();
        }
    } else if (bridge_type_ == kTypeBpEnable) {
        if (!computer || !computer->breakpoints) {
            bridge_error_ = kEInternal;
        } else if (bridge_arg1_ != 0 && bridge_arg1_ != 1) {
            bridge_error_ = kEBadLength;
        } else if (!computer->breakpoints->set_enabled(bridge_arg0_, bridge_arg1_ != 0)) {
            bridge_error_ = kEInternal;
            bridge_error_text_ = "unknown id";
        }
    } else if (bridge_type_ == kTypeBpList) {
        if (!computer || !computer->breakpoints) {
            bridge_error_ = kEInternal;
        } else {
            const auto &entries = computer->breakpoints->entries();
            const uint32_t count = static_cast<uint32_t>(entries.size());
            bridge_reply_.resize(4 + count * kBpListRecordSize);
            std::memcpy(bridge_reply_.data(), &count, 4);
            for (uint32_t i = 0; i < count; ++i) {
                const bp_entry_t &e = entries[i];
                uint8_t *rec = bridge_reply_.data() + 4 + i * kBpListRecordSize;
                std::memcpy(rec + 0, &e.id, 4);
                std::memcpy(rec + 4, &e.hit_count, 4);
                pack_bp_fields(rec + 8, e);
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
    bridge_megaii_platform_reject_ = false;
    bridge_error_text_.clear();
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

void DebugProtocolServer::enqueue_event(uint32_t event_id, const std::vector<uint8_t> &data) {
    std::vector<uint8_t> item(4 + data.size());
    std::memcpy(item.data(), &event_id, 4);
    if (!data.empty()) {
        std::memcpy(item.data() + 4, data.data(), data.size());
    }
    std::lock_guard<std::mutex> lock(event_mu_);
    event_queue_.push_back(std::move(item));
}

void DebugProtocolServer::fill_live_trace(computer_t *computer, system_trace_entry_t *out) {
    if (!out) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    if (!computer || !computer->cpu) {
        return;
    }
    cpu_state *cpu = computer->cpu;
    if (computer->clock) {
        out->cycle = computer->clock->get_cycles();
    }
    out->pc = cpu->pc;
    out->pb = cpu->pb;
    out->db = cpu->db;
    out->a = cpu->a;
    out->x = cpu->x;
    out->y = cpu->y;
    out->sp = cpu->sp;
    out->d = cpu->d;
    out->p = cpu->p;
    out->unused = 0;
    if (cpu->mmu) {
        out->opcode = cpu->mmu->read(cpu->full_pc);
    }
}

void DebugProtocolServer::pack_stopped_event(std::vector<uint8_t> &out, const StopHit &hit,
                                             uint32_t execution_mode, const system_trace_entry_t &trace) {
    constexpr uint32_t kHeaderSize = 32;
    constexpr uint32_t kTraceSize = 40;
    out.resize(kHeaderSize + kTraceSize);

    const uint32_t trace_size = kTraceSize;
    const uint16_t pad16 = 0;

    std::memcpy(out.data() + 0, &hit.reason, 4);
    std::memcpy(out.data() + 4, &hit.bp_id, 4);
    std::memcpy(out.data() + 8, &hit.pc, 4);
    std::memcpy(out.data() + 12, &hit.eaddr, 4);
    std::memcpy(out.data() + 16, &hit.value, 4);
    out[20] = hit.access;
    out[21] = hit.kind;
    std::memcpy(out.data() + 22, &pad16, 2);
    std::memcpy(out.data() + 24, &execution_mode, 4);
    std::memcpy(out.data() + 28, &trace_size, 4);
    std::memcpy(out.data() + kHeaderSize, &trace, kTraceSize);
}

void DebugProtocolServer::emit_stopped(computer_t *computer, const StopHit &hit) {
    g_last_stop_reason = hit.reason;
    system_trace_entry_t trace{};
    if (hit.reason == STOP_BP_EXEC || hit.reason == STOP_PAUSE) {
        fill_live_trace(computer, &trace);
    } else if (computer && computer->cpu) {
        trace = computer->cpu->trace_entry;
    }
    const uint32_t mode = computer
        ? static_cast<uint32_t>(computer->execution_mode)
        : static_cast<uint32_t>(EXEC_PAUSED);
    std::vector<uint8_t> data;
    pack_stopped_event(data, hit, mode, trace);
    enqueue_event(kEvtStopped, data);
}

void DebugProtocolServer::emit_stopped_pause(computer_t *computer) {
    StopHit hit{};
    hit.reason = STOP_PAUSE;
    if (computer && computer->cpu) {
        hit.pc = computer->cpu->full_pc;
    }
    g_last_stop_reason = STOP_PAUSE;
    system_trace_entry_t trace{};
    fill_live_trace(computer, &trace);
    const uint32_t mode = computer
        ? static_cast<uint32_t>(computer->execution_mode)
        : static_cast<uint32_t>(EXEC_PAUSED);
    std::vector<uint8_t> data;
    pack_stopped_event(data, hit, mode, trace);
    enqueue_event(kEvtStopped, data);
}

void DebugProtocolServer::emit_stopped_step(computer_t *computer) {
    StopHit hit{};
    hit.reason = STOP_STEP;
    system_trace_entry_t trace{};
    if (computer && computer->cpu) {
        trace = computer->cpu->trace_entry;
        hit.pc = (static_cast<uint32_t>(trace.pb) << 16) | trace.pc;
        hit.eaddr = trace.eaddr;
        hit.value = static_cast<uint32_t>(trace.data & 0xFF);
    }
    g_last_stop_reason = STOP_STEP;
    const uint32_t mode = computer
        ? static_cast<uint32_t>(computer->execution_mode)
        : static_cast<uint32_t>(EXEC_STEP_INTO);
    std::vector<uint8_t> data;
    pack_stopped_event(data, hit, mode, trace);
    enqueue_event(kEvtStopped, data);
}

void DebugProtocolServer::emit_run_state(uint32_t new_mode, uint32_t prev_mode) {
    std::vector<uint8_t> data(8);
    std::memcpy(data.data() + 0, &new_mode, 4);
    std::memcpy(data.data() + 4, &prev_mode, 4);
    enqueue_event(kEvtRunState, data);
}

bool DebugProtocolServer::flush_events(int fd) {
#if GS2_DEBUG_PROTO_UNIX
    std::deque<std::vector<uint8_t>> pending;
    {
        std::lock_guard<std::mutex> lock(event_mu_);
        pending.swap(event_queue_);
    }
    for (const auto &item : pending) {
        if (!send_frame(fd, kTypeEvent, event_seq_++, item.data(), static_cast<uint32_t>(item.size()))) {
            return false;
        }
    }
    return true;
#else
    (void)fd;
    return true;
#endif
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd{};
                pfd.fd = fd;
                pfd.events = POLLIN;
                const int pr = poll(&pfd, 1, 50);
                if (pr < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                if (pr == 0) {
                    if (stop_) {
                        return false;
                    }
                    continue;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    return false;
                }
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pfd{};
                pfd.fd = fd;
                pfd.events = POLLOUT;
                const int pr = poll(&pfd, 1, 50);
                if (pr < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    return false;
                }
                if (pr == 0) {
                    if (stop_) {
                        return false;
                    }
                    continue;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    return false;
                }
                continue;
            }
            return false;
        }
        if (w == 0) {
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

bool DebugProtocolServer::reject(int fd, uint32_t seq, uint32_t code, const char *message) {
    return !send_error(fd, seq, code, message);
}

const char *DebugProtocolServer::bridge_error_message(uint32_t err, uint32_t domain) {
    if (err == kEBusy) {
        return "busy";
    }
    if (err == kEBadLength || err == kEInternal) {
        std::lock_guard<std::mutex> lock(bridge_mu_);
        if (!bridge_error_text_.empty()) {
            return bridge_error_text_.c_str();
        }
    }
    if (err == kEBadLength) {
        return "out of range";
    }
    if (err == kEInternal) {
        std::lock_guard<std::mutex> lock(bridge_mu_);
        if (bridge_timed_out_) {
            return "timeout waiting for main thread";
        }
        if (bridge_megaii_platform_reject_) {
            return "MEGAII only on Apple IIgs";
        }
        if (domain != kMemMain && domain != kMemMegaII
            && domain != kMemMainRaw && domain != kMemMegaIIRaw) {
            return "unsupported domain";
        }
        return "no machine";
    }
    return "internal error";
}

void DebugProtocolServer::serve_client(int client_fd) {
#if GS2_DEBUG_PROTO_UNIX
    const int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
    bool handshaked = false;

    // Reject request with an error frame; disconnect on write failure, else next request.
    // Use `continue` via a statement expression carefully: REJECT must continue the
    // serve_client loop, not a do-while(0) wrapper (continue would bind to that).
#define REJECT(...) do { if (reject(__VA_ARGS__)) return; goto next_request; } while (0)
#define REPLY_OK(type, seq, payload_ptr, payload_len) do { \
        if (!send_frame(client_fd, (type), (seq), (payload_ptr), (payload_len))) { \
            return; \
        } \
        if (!flush_events(client_fd)) { \
            return; \
        } \
    } while (0)

    while (!stop_) {
next_request:
        if (!flush_events(client_fd)) {
            return;
        }

#if GS2_DEBUG_PROTO_UNIX
        struct pollfd pfd{};
        pfd.fd = client_fd;
        pfd.events = POLLIN;
        const int pr = poll(&pfd, 1, 50);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            return;
        }
        if (pr == 0) {
            continue;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            return;
        }
        if (!(pfd.revents & POLLIN)) {
            continue;
        }
#endif

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
            REJECT(client_fd, hdr.seq, kEUnknownType, "flags must be zero on requests");
        }

        if (!handshaked && hdr.type != kTypeHello) {
            REJECT(client_fd, hdr.seq, kENotHandshaked, "HELLO required first");
        }

        switch (hdr.type) {
        case kTypeHello: {
            if (hdr.length != 8) {
                REJECT(client_fd, hdr.seq, kEBadLength, "HELLO requires 8-byte payload");
            }
            uint32_t client_version = 0;
            std::memcpy(&client_version, payload.data(), 4);
            if (client_version != kProtoVersion) {
                REJECT(client_fd, hdr.seq, kEBadVersion, "unsupported protocol version");
            }
            uint8_t reply[12];
            uint32_t version = kProtoVersion;
            uint32_t flags = 0;
            uint32_t max_payload = kMaxPayload;
            std::memcpy(reply + 0, &version, 4);
            std::memcpy(reply + 4, &flags, 4);
            std::memcpy(reply + 8, &max_payload, 4);
            REPLY_OK(kTypeHello, hdr.seq, reply, 12);
            handshaked = true;
            break;
        }
        case kTypePing: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "PING requires empty payload");
            }
            REPLY_OK(kTypePing, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeQuit: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "QUIT requires empty payload");
            }
            // Reply before scheduling halt so the client gets ACK while the
            // socket is still alive. AppQuit (same frame as halt) stops the
            // protocol thread and would otherwise race the reply.
            REPLY_OK(kTypeQuit, hdr.seq, nullptr, 0);
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeQuit, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            // Ignore bridge errors after ACK — process may already be exiting.
            break;
        }
        case kTypeGetStatus: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "GET_STATUS requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeGetStatus, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() != 8) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad status reply");
            }
            REPLY_OK(kTypeGetStatus, hdr.seq, reply.data(), 8);
            break;
        }
        case kTypeReset: {
            if (hdr.length != 4) {
                REJECT(client_fd, hdr.seq, kEBadLength, "RESET requires 4-byte payload");
            }
            uint32_t cold_start = 0;
            std::memcpy(&cold_start, payload.data(), 4);
            if (cold_start != 0 && cold_start != 1) {
                REJECT(client_fd, hdr.seq, kEBadLength, "RESET cold_start must be 0 or 1");
            }

            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeReset, hdr.seq, cold_start, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad reset reply");
            }
            REPLY_OK(kTypeReset, hdr.seq, nullptr, 0);
            break;
        }
        case kTypePause: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "PAUSE requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypePause, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad pause reply");
            }
            REPLY_OK(kTypePause, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeContinue: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "CONTINUE requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeContinue, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad continue reply");
            }
            REPLY_OK(kTypeContinue, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeStepInto: {
            if (hdr.length != 4) {
                REJECT(client_fd, hdr.seq, kEBadLength, "STEP_INTO requires 4-byte payload");
            }
            uint32_t count = 0;
            std::memcpy(&count, payload.data(), 4);
            if (count == 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "STEP_INTO count must be >= 1");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeStepInto, hdr.seq, count, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad step_into reply");
            }
            REPLY_OK(kTypeStepInto, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeReadMem: {
            if (hdr.length != 12) {
                REJECT(client_fd, hdr.seq, kEBadLength, "READMEM requires 12-byte payload");
            }
            uint32_t domain = 0, address = 0, length = 0;
            std::memcpy(&domain, payload.data() + 0, 4);
            std::memcpy(&address, payload.data() + 4, 4);
            std::memcpy(&length, payload.data() + 8, 4);

            if (length == 0 || length > kMaxReadMem) {
                REJECT(client_fd, hdr.seq, kEBadLength, "READMEM length out of range");
            }
            if (address > std::numeric_limits<uint32_t>::max() - length) {
                REJECT(client_fd, hdr.seq, kEBadLength, "READMEM address wrap");
            }
            if (domain != kMemMain && domain != kMemMegaII && domain != kMemEnsoniq
                && domain != kMemAdbMicro && domain != kMemMainRaw && domain != kMemMegaIIRaw) {
                REJECT(client_fd, hdr.seq, kEBadLength, "READMEM invalid domain");
            }

            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeReadMem, hdr.seq, domain, address, length, kEmptyRequest, reply,
                                 err, kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err, domain));
            }
            if (reply.size() != length) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad readmem reply");
            }
            REPLY_OK(kTypeReadMem, hdr.seq, reply.data(), length);
            break;
        }
        case kTypeWriteMem: {
            if (hdr.length < 12) {
                REJECT(client_fd, hdr.seq, kEBadLength, "WRITEMEM payload too short");
            }
            uint32_t domain = 0, address = 0, length = 0;
            std::memcpy(&domain, payload.data() + 0, 4);
            std::memcpy(&address, payload.data() + 4, 4);
            std::memcpy(&length, payload.data() + 8, 4);

            if (length == 0 || length > kMaxWriteMem) {
                REJECT(client_fd, hdr.seq, kEBadLength, "WRITEMEM length out of range");
            }
            if (hdr.length != 12 + length) {
                REJECT(client_fd, hdr.seq, kEBadLength, "WRITEMEM payload length mismatch");
            }
            if (address > std::numeric_limits<uint32_t>::max() - length) {
                REJECT(client_fd, hdr.seq, kEBadLength, "WRITEMEM address wrap");
            }
            if (domain != kMemMain && domain != kMemMegaII && domain != kMemEnsoniq
                && domain != kMemAdbMicro && domain != kMemMainRaw && domain != kMemMegaIIRaw) {
                REJECT(client_fd, hdr.seq, kEBadLength, "WRITEMEM invalid domain");
            }

            std::vector<uint8_t> request_data(payload.begin() + 12, payload.end());
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeWriteMem, hdr.seq, domain, address, length, request_data, reply,
                                 err, kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err, domain));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad writemem reply");
            }
            REPLY_OK(kTypeWriteMem, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeBpSet: {
            if (hdr.length != kBpSetPayloadSize) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_SET requires 32-byte payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeBpSet, hdr.seq, 0, 0, 0, payload, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() != 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_set reply");
            }
            REPLY_OK(kTypeBpSet, hdr.seq, reply.data(), 4);
            break;
        }
        case kTypeBpClear: {
            if (hdr.length != 4) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_CLEAR requires 4-byte payload");
            }
            uint32_t id = 0;
            std::memcpy(&id, payload.data(), 4);
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeBpClear, hdr.seq, id, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_clear reply");
            }
            REPLY_OK(kTypeBpClear, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeBpClearAll: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_CLEAR_ALL requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeBpClearAll, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_clear_all reply");
            }
            REPLY_OK(kTypeBpClearAll, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeBpEnable: {
            if (hdr.length != 8) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_ENABLE requires 8-byte payload");
            }
            uint32_t id = 0;
            uint32_t enabled = 0;
            std::memcpy(&id, payload.data() + 0, 4);
            std::memcpy(&enabled, payload.data() + 4, 4);
            if (enabled != 0 && enabled != 1) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_ENABLE enabled must be 0 or 1");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeBpEnable, hdr.seq, id, enabled, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_enable reply");
            }
            REPLY_OK(kTypeBpEnable, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeBpList: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "BP_LIST requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeBpList, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() < 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_list reply");
            }
            uint32_t count = 0;
            std::memcpy(&count, reply.data(), 4);
            if (reply.size() != 4 + count * kBpListRecordSize) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad bp_list reply");
            }
            REPLY_OK(kTypeBpList, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
            break;
        }
        case kTypeKeyEvent: {
            if (hdr.length != 12) {
                REJECT(client_fd, hdr.seq, kEBadLength, "KEYEVENT requires 12-byte payload");
            }
            uint32_t down = 0, scancode = 0, mod = 0;
            std::memcpy(&down, payload.data() + 0, 4);
            std::memcpy(&scancode, payload.data() + 4, 4);
            std::memcpy(&mod, payload.data() + 8, 4);

            if (down != 0 && down != 1) {
                REJECT(client_fd, hdr.seq, kEBadLength, "KEYEVENT down must be 0 or 1");
            }

            SDL_Event ev{};
            ev.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            ev.key.scancode = static_cast<SDL_Scancode>(scancode);
            ev.key.mod = static_cast<SDL_Keymod>(mod);
            ev.key.key = SDL_GetKeyFromScancode(ev.key.scancode, ev.key.mod, false);
            ev.key.down = (down != 0);
            ev.key.repeat = false;

            if (!SDL_PushEvent(&ev)) {
                REJECT(client_fd, hdr.seq, kEInternal, "SDL_PushEvent failed");
            }
            REPLY_OK(kTypeKeyEvent, hdr.seq, nullptr, 0);
            break;
        }
        default: {
            REJECT(client_fd, hdr.seq, kEUnknownType, "unknown type");
        }
        }
    }

#undef REPLY_OK
#undef REJECT
}
