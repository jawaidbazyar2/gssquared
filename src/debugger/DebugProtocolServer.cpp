#include "debugger/DebugProtocolServer.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <afunix.h>
#define GS2_DEBUG_PROTO_UNIX 1
#elif !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#define GS2_DEBUG_PROTO_UNIX 1
#else
#define GS2_DEBUG_PROTO_UNIX 0
#endif

#include <SDL3/SDL.h>

#include "computer.hpp"
#include "cpu.hpp"
#include "Device_ID.hpp"
#include "devices/es5503/soundglu.hpp"
#include "display/display.hpp"
#include "display/text_page_layout.hpp"
#include "gs2.hpp"
#include "mmus/mmu.hpp"
#include "Module_ID.hpp"
#include "PlatformIDs.hpp"
#include "util/mount.hpp"
#include "util/StorageDevice.hpp"

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
constexpr uint32_t kTypeGetTrace  = 0x00000201;
constexpr uint32_t kTypeGetRegs   = 0x00000202;
constexpr uint32_t kTypeSetRegs   = 0x00000203;
constexpr uint32_t kTypeReadMem   = 0x00000301;
constexpr uint32_t kTypeWriteMem  = 0x00000302;
constexpr uint32_t kTypeFindMem   = 0x00000303;
constexpr uint32_t kTypeStateGet  = 0x00000601;
constexpr uint32_t kTypeStateSet  = 0x00000602;
constexpr uint32_t kTypeBpSet     = 0x00000401;
constexpr uint32_t kTypeBpClear   = 0x00000402;
constexpr uint32_t kTypeBpClearAll = 0x00000403;
constexpr uint32_t kTypeBpEnable  = 0x00000404;
constexpr uint32_t kTypeBpList    = 0x00000405;
constexpr uint32_t kTypeKeyEvent  = 0x00000501;
constexpr uint32_t kTypePasteText = 0x00000502;
constexpr uint32_t kTypeVideoText = 0x00000701;
constexpr uint32_t kTypeMount     = 0x00000801;
constexpr uint32_t kTypeUnmount   = 0x00000802;

constexpr uint32_t kEvtStopped   = 1;
constexpr uint32_t kEvtRunState  = 2;

constexpr uint32_t kBpSetPayloadSize = 32;
constexpr uint32_t kBpListRecordSize = 40;
constexpr uint32_t kTraceEntrySize = 40;
constexpr uint32_t kMaxTraceRecords = 16384;
constexpr uint32_t kDocRamSize = 0x10000;
constexpr uint32_t kSetRegsPayloadSize = 24;
constexpr uint32_t kFindMemHeaderSize = 24;
constexpr uint32_t kMaxFindPattern = 256;
constexpr uint32_t kMaxFindHits = 256;

constexpr uint32_t kRegPc = 1u << 0;
constexpr uint32_t kRegPb = 1u << 1;
constexpr uint32_t kRegDb = 1u << 2;
constexpr uint32_t kRegA  = 1u << 3;
constexpr uint32_t kRegX  = 1u << 4;
constexpr uint32_t kRegY  = 1u << 5;
constexpr uint32_t kRegSp = 1u << 6;
constexpr uint32_t kRegD  = 1u << 7;
constexpr uint32_t kRegP  = 1u << 8;
constexpr uint32_t kRegE  = 1u << 9;
constexpr uint32_t kRegMaskAll =
    kRegPc | kRegPb | kRegDb | kRegA | kRegX | kRegY | kRegSp | kRegD | kRegP | kRegE;

constexpr uint32_t kFindMemHasMask = 1u << 0;

constexpr uint32_t kVideoPageCurrent = 0;
constexpr uint32_t kVideoModeCurrent = 0;
constexpr uint32_t kVideoModeText40 = 1;
constexpr uint32_t kVideoModeText80 = 2;
constexpr uint32_t kVideoTextHeaderSize = 20;
constexpr uint32_t kVideoTextReqSize = 8;

constexpr uint32_t kVfText    = 1u << 0;
constexpr uint32_t kVfMix     = 1u << 1;
constexpr uint32_t kVfPage2   = 1u << 2;
constexpr uint32_t kVfHires   = 1u << 3;
constexpr uint32_t kVf80Col   = 1u << 4;
constexpr uint32_t kVfAltChar = 1u << 5;

constexpr uint32_t kMediaOk            = 0;
constexpr uint32_t kMediaNoDrive       = 1;
constexpr uint32_t kMediaMountFailed   = 2;
constexpr uint32_t kMediaUnmountFailed = 3;
constexpr uint32_t kMediaBadPath       = 4;
constexpr uint32_t kMaxMediaPathLen    = 4096;
constexpr uint32_t kMaxMediaUnit       = 5;

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
constexpr DebugSocketHandle kInvalidSocket = -1;

#if defined(_WIN32)
SOCKET native_socket(DebugSocketHandle fd) {
    return static_cast<SOCKET>(fd);
}

DebugSocketHandle debug_socket(SOCKET fd) {
    return fd == INVALID_SOCKET ? kInvalidSocket : static_cast<DebugSocketHandle>(fd);
}

void close_socket(DebugSocketHandle fd) {
    if (fd != kInvalidSocket) {
        ::closesocket(native_socket(fd));
    }
}

void shutdown_socket(DebugSocketHandle fd) {
    if (fd != kInvalidSocket) {
        ::shutdown(native_socket(fd), SD_BOTH);
    }
}
#else
void close_socket(DebugSocketHandle fd) {
    if (fd != kInvalidSocket) {
        ::close(static_cast<int>(fd));
    }
}

void shutdown_socket(DebugSocketHandle fd) {
    if (fd != kInvalidSocket) {
        ::shutdown(static_cast<int>(fd), SHUT_RDWR);
    }
}
#endif

void remove_socket_path(const std::string &path) {
    if (!path.empty()) {
        std::remove(path.c_str());
    }
}

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

/** Peek `length` bytes from a READMEM domain into `out`. On failure sets error fields. */
bool read_domain_bytes(computer_t *computer, uint32_t domain, uint32_t address, uint32_t length,
                       std::vector<uint8_t> &out, uint32_t &error, bool &megaii_reject,
                       std::string &error_text) {
    out.clear();
    error = 0;
    megaii_reject = false;
    error_text.clear();

    if (domain == kMemMain) {
        if (!computer || !computer->cpu || !computer->cpu->mmu) {
            error = kEInternal;
            return false;
        }
        MMU *mmu = computer->cpu->mmu;
        out.resize(length);
        for (uint32_t i = 0; i < length; ++i) {
            out[i] = mmu->read(address + i);
        }
        return true;
    }
    if (domain == kMemMegaII) {
        if (!computer || !computer->platform || !platform_is_iigs(computer->platform->id)) {
            error = kEInternal;
            megaii_reject = true;
            return false;
        }
        if (!computer->mmu) {
            error = kEInternal;
            return false;
        }
        MMU *mmu = computer->mmu;
        out.resize(length);
        for (uint32_t i = 0; i < length; ++i) {
            out[i] = mmu->read(address + i);
        }
        return true;
    }
    if (domain == kMemMainRaw) {
        if (!computer || !computer->cpu || !computer->cpu->mmu) {
            error = kEInternal;
            return false;
        }
        MMU *mmu = computer->cpu->mmu;
        uint8_t *base = mmu->get_memory_base();
        uint32_t size = mmu->get_memory_size();
        if (!base || size == 0) {
            error = kEInternal;
            return false;
        }
        if (address > size || length > size - address) {
            error = kEBadLength;
            return false;
        }
        out.assign(base + address, base + address + length);
        return true;
    }
    if (domain == kMemMegaIIRaw) {
        if (!computer || !computer->platform || !platform_is_iigs(computer->platform->id)) {
            error = kEInternal;
            megaii_reject = true;
            return false;
        }
        if (!computer->mmu) {
            error = kEInternal;
            return false;
        }
        MMU *mmu = computer->mmu;
        uint8_t *base = mmu->get_memory_base();
        uint32_t size = mmu->get_memory_size();
        if (!base || size == 0) {
            error = kEInternal;
            return false;
        }
        if (address > size || length > size - address) {
            error = kEBadLength;
            return false;
        }
        out.assign(base + address, base + address + length);
        return true;
    }
    if (domain == kMemEnsoniq) {
        if (!computer || !computer->platform || computer->platform->id != PLATFORM_APPLE_IIGS) {
            error = kEInternal;
            return false;
        }
        auto *st = static_cast<ensoniq_state_t *>(computer->module_store[MODULE_ENSONIQ]);
        if (!st || !st->doc_ram) {
            error = kEInternal;
            error_text = "no ensoniq";
            return false;
        }
        if (address > kDocRamSize || length > kDocRamSize - address) {
            error = kEBadLength;
            return false;
        }
        out.assign(st->doc_ram + address, st->doc_ram + address + length);
        return true;
    }
    error = kEInternal;
    return false;
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
#if defined(_WIN32)
    WSADATA wsa_data{};
    const int startup_error = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (startup_error != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DebugProtocolServer: WSAStartup(): %d", startup_error);
        return false;
    }
    winsock_started_ = true;
    const DebugSocketHandle lfd = debug_socket(::socket(AF_UNIX, SOCK_STREAM, 0));
#else
    const DebugSocketHandle lfd = ::socket(AF_UNIX, SOCK_STREAM, 0);
#endif
    if (lfd == kInvalidSocket) {
#if defined(_WIN32)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DebugProtocolServer: socket(): %d", ::WSAGetLastError());
        ::WSACleanup();
        winsock_started_ = false;
#else
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: socket(): %s", strerror(errno));
#endif
        return false;
    }

    remove_socket_path(socket_path_);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socket_path_.size() >= sizeof(addr.sun_path)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: path too long: %s", socket_path_.c_str());
        close_socket(lfd);
#if defined(_WIN32)
        ::WSACleanup();
        winsock_started_ = false;
#endif
        return false;
    }
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path_.c_str());

#if defined(_WIN32)
    if (::bind(native_socket(lfd), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: bind(%s): %d",
                     socket_path_.c_str(), ::WSAGetLastError());
#else
    if (::bind(static_cast<int>(lfd), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: bind(%s): %s",
                     socket_path_.c_str(), strerror(errno));
#endif
        close_socket(lfd);
        remove_socket_path(socket_path_);
#if defined(_WIN32)
        ::WSACleanup();
        winsock_started_ = false;
#endif
        return false;
    }

#if defined(_WIN32)
    if (::listen(native_socket(lfd), 1) == SOCKET_ERROR) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "DebugProtocolServer: listen(): %d", ::WSAGetLastError());
#else
    if (::listen(static_cast<int>(lfd), 1) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: listen(): %s", strerror(errno));
#endif
        close_socket(lfd);
        remove_socket_path(socket_path_);
#if defined(_WIN32)
        ::WSACleanup();
        winsock_started_ = false;
#endif
        return false;
    }

    listen_fd_ = lfd;
    thread_ = SDL_CreateThread(thread_entry, "gs2-debug-proto", this);
    if (!thread_) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DebugProtocolServer: SDL_CreateThread failed: %s",
                     SDL_GetError());
        listen_fd_ = kInvalidSocket;
        close_socket(lfd);
        remove_socket_path(socket_path_);
#if defined(_WIN32)
        ::WSACleanup();
        winsock_started_ = false;
#endif
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
    const DebugSocketHandle cfd = client_fd_.exchange(kInvalidSocket);
    shutdown_socket(cfd);
    const DebugSocketHandle lfd = listen_fd_.exchange(kInvalidSocket);
    shutdown_socket(lfd);
    close_socket(lfd);
#endif
    if (thread_) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
#if GS2_DEBUG_PROTO_UNIX
    remove_socket_path(socket_path_);
#endif
#if defined(_WIN32)
    if (winsock_started_) {
        ::WSACleanup();
        winsock_started_ = false;
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
        if (!read_domain_bytes(computer, domain, address, length, bridge_reply_, bridge_error_,
                               bridge_megaii_platform_reject_, bridge_error_text_)) {
            // bridge_error_ / flags already set
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
                || !platform_is_iigs(computer->platform->id)) {
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
                || !platform_is_iigs(computer->platform->id)) {
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
        } else if (domain == kMemEnsoniq) {
            if (!computer || !computer->platform
                || computer->platform->id != PLATFORM_APPLE_IIGS) {
                bridge_error_ = kEInternal;
            } else if (bridge_request_.size() != length) {
                bridge_error_ = kEInternal;
            } else {
                auto *st = static_cast<ensoniq_state_t *>(computer->module_store[MODULE_ENSONIQ]);
                if (!st || !st->doc_ram) {
                    bridge_error_ = kEInternal;
                    bridge_error_text_ = "no ensoniq";
                } else if (address > kDocRamSize || length > kDocRamSize - address) {
                    bridge_error_ = kEBadLength;
                } else {
                    std::memcpy(st->doc_ram + address, bridge_request_.data(), length);
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
    } else if (bridge_type_ == kTypeGetTrace) {
        if (!computer || !computer->cpu || !computer->cpu->trace_buffer) {
            bridge_error_ = kEInternal;
        } else {
            const uint32_t ago = bridge_arg0_;
            const uint32_t want = bridge_arg1_;
            system_trace_buffer *tb = computer->cpu->trace_buffer;
            const uint32_t available = static_cast<uint32_t>(tb->count);
            uint32_t returned = 0;
            if (ago < available && want > 0) {
                const uint32_t max_from_ago = available - ago;
                returned = want < max_from_ago ? want : max_from_ago;
            }
            bridge_reply_.resize(8 + static_cast<size_t>(returned) * kTraceEntrySize);
            std::memcpy(bridge_reply_.data() + 0, &available, 4);
            std::memcpy(bridge_reply_.data() + 4, &returned, 4);
            if (returned > 0) {
                const size_t sz = tb->size;
                size_t idx = (tb->head + sz - static_cast<size_t>(ago) - static_cast<size_t>(returned)) % sz;
                uint8_t *out = bridge_reply_.data() + 8;
                for (uint32_t i = 0; i < returned; ++i) {
                    std::memcpy(out + static_cast<size_t>(i) * kTraceEntrySize,
                                &tb->entries[idx], kTraceEntrySize);
                    idx++;
                    if (idx >= sz) {
                        idx = 0;
                    }
                }
            }
        }
    } else if (bridge_type_ == kTypeGetRegs) {
        if (!computer || !computer->cpu) {
            bridge_error_ = kEInternal;
        } else {
            system_trace_entry_t live{};
            fill_live_trace(computer, &live);
            bridge_reply_.resize(kTraceEntrySize);
            std::memcpy(bridge_reply_.data(), &live, kTraceEntrySize);
        }
    } else if (bridge_type_ == kTypeSetRegs) {
        if (!computer || !computer->cpu) {
            bridge_error_ = kEInternal;
        } else if (bridge_request_.size() != kSetRegsPayloadSize) {
            bridge_error_ = kEBadLength;
        } else {
            uint32_t mask = 0;
            uint16_t pc = 0, a = 0, x = 0, y = 0, sp = 0, d = 0;
            uint8_t pb = 0, db = 0, p = 0, e = 0;
            std::memcpy(&mask, bridge_request_.data() + 0, 4);
            std::memcpy(&pc, bridge_request_.data() + 4, 2);
            pb = bridge_request_[6];
            db = bridge_request_[7];
            std::memcpy(&a, bridge_request_.data() + 8, 2);
            std::memcpy(&x, bridge_request_.data() + 10, 2);
            std::memcpy(&y, bridge_request_.data() + 12, 2);
            std::memcpy(&sp, bridge_request_.data() + 14, 2);
            std::memcpy(&d, bridge_request_.data() + 16, 2);
            p = bridge_request_[18];
            e = bridge_request_[19];
            if ((mask & ~kRegMaskAll) != 0) {
                bridge_error_ = kEBadLength;
                bridge_error_text_ = "SET_REGS unknown mask bits";
            } else if ((mask & kRegE) && e > 1) {
                bridge_error_ = kEBadLength;
                bridge_error_text_ = "SET_REGS e must be 0 or 1";
            } else {
                cpu_state *cpu = computer->cpu;
                if (mask & kRegPc) {
                    cpu->pc = pc;
                }
                if (mask & kRegPb) {
                    cpu->pb = pb;
                }
                if (mask & kRegDb) {
                    cpu->db = db;
                }
                if (mask & kRegA) {
                    cpu->a = a;
                }
                if (mask & kRegX) {
                    cpu->x = x;
                }
                if (mask & kRegY) {
                    cpu->y = y;
                }
                if (mask & kRegSp) {
                    cpu->sp = sp;
                }
                if (mask & kRegD) {
                    cpu->d = d;
                }
                if (mask & kRegP) {
                    cpu->p = p;
                }
                if (mask & kRegE) {
                    cpu->E = e;
                }
            }
        }
    } else if (bridge_type_ == kTypeFindMem) {
        if (bridge_request_.size() < kFindMemHeaderSize) {
            bridge_error_ = kEBadLength;
        } else {
            uint32_t domain = 0, address = 0, length = 0, max_hits = 0, pattern_len = 0, flags = 0;
            std::memcpy(&domain, bridge_request_.data() + 0, 4);
            std::memcpy(&address, bridge_request_.data() + 4, 4);
            std::memcpy(&length, bridge_request_.data() + 8, 4);
            std::memcpy(&max_hits, bridge_request_.data() + 12, 4);
            std::memcpy(&pattern_len, bridge_request_.data() + 16, 4);
            std::memcpy(&flags, bridge_request_.data() + 20, 4);
            const bool has_mask = (flags & kFindMemHasMask) != 0;
            const size_t expect = kFindMemHeaderSize + pattern_len + (has_mask ? pattern_len : 0);
            if ((flags & ~kFindMemHasMask) != 0 || pattern_len == 0 || pattern_len > kMaxFindPattern
                || max_hits == 0 || max_hits > kMaxFindHits || pattern_len > length
                || bridge_request_.size() != expect) {
                bridge_error_ = kEBadLength;
            } else {
                std::vector<uint8_t> window;
                if (!read_domain_bytes(computer, domain, address, length, window, bridge_error_,
                                       bridge_megaii_platform_reject_, bridge_error_text_)) {
                    // error already set
                } else {
                    const uint8_t *pattern = bridge_request_.data() + kFindMemHeaderSize;
                    const uint8_t *mask = has_mask ? pattern + pattern_len : nullptr;
                    std::vector<uint32_t> hits;
                    hits.reserve(max_hits);
                    const uint32_t last = length - pattern_len;
                    for (uint32_t off = 0; off <= last && hits.size() < max_hits; ++off) {
                        bool match = true;
                        for (uint32_t j = 0; j < pattern_len; ++j) {
                            const uint8_t m = mask ? mask[j] : 0xFFu;
                            if ((window[off + j] & m) != (pattern[j] & m)) {
                                match = false;
                                break;
                            }
                        }
                        if (match) {
                            hits.push_back(address + off);
                        }
                    }
                    const uint32_t hit_count = static_cast<uint32_t>(hits.size());
                    bridge_reply_.resize(4 + hit_count * 4);
                    std::memcpy(bridge_reply_.data(), &hit_count, 4);
                    for (uint32_t i = 0; i < hit_count; ++i) {
                        std::memcpy(bridge_reply_.data() + 4 + i * 4, &hits[i], 4);
                    }
                }
            }
        }
    } else if (bridge_type_ == kTypeStateGet) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            const auto id = static_cast<device_id>(bridge_arg0_);
            std::string err;
            if (!computer->call_device_debug(id, DEVOP_STATE_GET, bridge_request_, bridge_reply_, err)) {
                bridge_error_ = kEInternal;
                bridge_error_text_ = err.empty() ? "unknown device" : err;
            }
        }
    } else if (bridge_type_ == kTypeStateSet) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else {
            const auto id = static_cast<device_id>(bridge_arg0_);
            std::string err;
            if (!computer->call_device_debug(id, DEVOP_STATE_SET, bridge_request_, bridge_reply_, err)) {
                bridge_error_ = kEInternal;
                bridge_error_text_ = err.empty() ? "unknown device" : err;
            }
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
    } else if (bridge_type_ == kTypeVideoText) {
        const uint32_t req_page = bridge_arg0_;
        const uint32_t req_mode = bridge_arg1_;
        auto *ds = static_cast<display_state_t *>(computer ? computer->cached_display_state : nullptr);
        if (!computer || !ds) {
            bridge_error_ = kEInternal;
        } else {
            uint32_t flags = 0;
            if (ds->display_mode == TEXT_MODE) {
                flags |= kVfText;
            }
            if (ds->display_split_mode == SPLIT_SCREEN) {
                flags |= kVfMix;
            }
            if (ds->display_page_num == DISPLAY_PAGE_2) {
                flags |= kVfPage2;
            }
            if (ds->display_graphics_mode == HIRES_MODE) {
                flags |= kVfHires;
            }
            if (ds->f_80col) {
                flags |= kVf80Col;
            }
            if (ds->f_altcharset) {
                flags |= kVfAltChar;
            }

            uint32_t page = req_page;
            if (page == kVideoPageCurrent) {
                page = (ds->display_page_num == DISPLAY_PAGE_2) ? 2u : 1u;
            } else if (page != 1 && page != 2) {
                bridge_error_ = kEBadLength;
                bridge_error_text_ = "VIDEO_TEXT invalid page";
            }

            uint32_t mode = req_mode;
            if (bridge_error_ == 0) {
                if (mode == kVideoModeCurrent) {
                    if (ds->display_mode != TEXT_MODE) {
                        bridge_error_ = kEInternal;
                        bridge_error_text_ = "current mode is not text";
                    } else {
                        mode = ds->f_80col ? kVideoModeText80 : kVideoModeText40;
                    }
                } else if (mode != kVideoModeText40 && mode != kVideoModeText80) {
                    bridge_error_ = kEBadLength;
                    bridge_error_text_ = "unsupported video mode";
                }
            }

            if (bridge_error_ == 0) {
                MMU *mmu = nullptr;
                if (computer->platform && platform_is_iigs(computer->platform->id)) {
                    mmu = computer->mmu;
                } else if (computer->cpu) {
                    mmu = computer->cpu->mmu;
                }
                if (!mmu) {
                    bridge_error_ = kEInternal;
                } else {
                    uint8_t *base = mmu->get_memory_base();
                    const uint32_t mem_size = mmu->get_memory_size();
                    const uint16_t page_off = text_page::page_base(page);
                    const uint32_t need_main = static_cast<uint32_t>(page_off) + 0x400;
                    if (!base || mem_size < need_main) {
                        bridge_error_ = kEInternal;
                    } else if (mode == kVideoModeText80
                               && mem_size < text_page::kAuxBankOffset + need_main) {
                        bridge_error_ = kEInternal;
                        bridge_error_text_ = "TEXT80 not available";
                    } else {
                        const uint32_t cols =
                            (mode == kVideoModeText80) ? text_page::kCols80 : text_page::kCols40;
                        const uint32_t rows = text_page::kRows;
                        const size_t chars_len = static_cast<size_t>(cols) * rows;
                        bridge_reply_.resize(kVideoTextHeaderSize + chars_len);
                        std::memcpy(bridge_reply_.data() + 0, &cols, 4);
                        std::memcpy(bridge_reply_.data() + 4, &rows, 4);
                        std::memcpy(bridge_reply_.data() + 8, &page, 4);
                        std::memcpy(bridge_reply_.data() + 12, &mode, 4);
                        std::memcpy(bridge_reply_.data() + 16, &flags, 4);
                        uint8_t *chars = bridge_reply_.data() + kVideoTextHeaderSize;
                        if (mode == kVideoModeText80) {
                            text_page::linearize_text80(base, page, chars);
                        } else {
                            text_page::linearize_text40(base, page, chars);
                        }
                    }
                }
            }
        }
    } else if (bridge_type_ == kTypeMount) {
        const uint32_t slot = bridge_arg0_;
        const uint32_t unit = bridge_arg1_;
        uint32_t status = kMediaOk;
        if (!computer || !computer->mounts) {
            bridge_error_ = kEInternal;
        } else if (bridge_request_.empty()) {
            status = kMediaBadPath;
        } else {
            storage_key_t key;
            key.slot = static_cast<uint16_t>(slot);
            key.drive = static_cast<uint16_t>(unit);
            key.partition = 0;
            key.subunit = 0;
            if (!computer->mounts->has_drive(key)) {
                status = kMediaNoDrive;
            } else {
                disk_mount_t dm{};
                dm.slot = static_cast<uint16_t>(slot);
                dm.drive = static_cast<uint16_t>(unit);
                dm.filename.assign(reinterpret_cast<const char *>(bridge_request_.data()),
                                   bridge_request_.size());
                if (!computer->mounts->mount_media(dm)) {
                    status = kMediaMountFailed;
                }
            }
        }
        if (bridge_error_ == 0) {
            bridge_reply_.resize(4);
            std::memcpy(bridge_reply_.data(), &status, 4);
        }
    } else if (bridge_type_ == kTypeUnmount) {
        const uint32_t slot = bridge_arg0_;
        const uint32_t unit = bridge_arg1_;
        uint32_t status = kMediaOk;
        if (!computer || !computer->mounts) {
            bridge_error_ = kEInternal;
        } else {
            storage_key_t key;
            key.slot = static_cast<uint16_t>(slot);
            key.drive = static_cast<uint16_t>(unit);
            key.partition = 0;
            key.subunit = 0;
            if (!computer->mounts->has_drive(key)) {
                status = kMediaNoDrive;
            } else if (!computer->mounts->unmount_media(key, DISCARD)) {
                status = kMediaUnmountFailed;
            }
        }
        if (bridge_error_ == 0) {
            bridge_reply_.resize(4);
            std::memcpy(bridge_reply_.data(), &status, 4);
        }
    } else if (bridge_type_ == kTypePasteText) {
        if (!computer) {
            bridge_error_ = kEInternal;
        } else if (!computer->start_keyboard_paste(
                       std::string(bridge_request_.begin(), bridge_request_.end()))) {
            bridge_error_ = kEInternal;
            bridge_error_text_ = "no keyboard";
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

bool DebugProtocolServer::flush_events(DebugSocketHandle fd) {
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
    const DebugSocketHandle lfd = listen_fd_.load();
    while (!stop_ && lfd != kInvalidSocket) {
#if defined(_WIN32)
        const DebugSocketHandle client_fd = debug_socket(::accept(native_socket(lfd), nullptr, nullptr));
        if (client_fd == kInvalidSocket) {
            const int socket_error = ::WSAGetLastError();
            if (stop_ || socket_error != WSAEINTR) {
                break;
            }
#else
        const DebugSocketHandle client_fd = ::accept(static_cast<int>(lfd), nullptr, nullptr);
        if (client_fd == kInvalidSocket) {
            if (stop_ || errno != EINTR) {
                break;
            }
#endif
            continue;
        }

        if (stop_) {
            close_socket(client_fd);
            break;
        }

        client_fd_ = client_fd;
        SDL_Log("DebugProtocolServer: client connected");
        serve_client(client_fd);
        client_fd_ = kInvalidSocket;
        close_socket(client_fd);
        SDL_Log("DebugProtocolServer: client disconnected");
    }
#endif
}

bool DebugProtocolServer::read_full(DebugSocketHandle fd, void *buf, size_t n) {
#if GS2_DEBUG_PROTO_UNIX
    auto *p = static_cast<uint8_t *>(buf);
    size_t got = 0;
    while (got < n) {
        if (stop_) {
            return false;
        }
#if defined(_WIN32)
        const size_t remaining = n - got;
        const int chunk = static_cast<int>(std::min(
            remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
        const int r = ::recv(native_socket(fd), reinterpret_cast<char *>(p + got), chunk, 0);
        if (r == 0) {
            return false;
        }
        if (r == SOCKET_ERROR) {
            const int socket_error = ::WSAGetLastError();
            if (socket_error == WSAEINTR) {
                continue;
            }
            if (socket_error == WSAEWOULDBLOCK) {
                WSAPOLLFD pfd{};
                pfd.fd = native_socket(fd);
                pfd.events = POLLRDNORM;
                const int pr = ::WSAPoll(&pfd, 1, 50);
                if (pr == SOCKET_ERROR) {
                    if (::WSAGetLastError() == WSAEINTR) {
                        continue;
                    }
                    return false;
                }
                if (pr == 0) {
                    continue;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    return false;
                }
                continue;
            }
            return false;
        }
#else
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
#endif
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

bool DebugProtocolServer::write_full(DebugSocketHandle fd, const void *buf, size_t n) {
#if GS2_DEBUG_PROTO_UNIX
    auto *p = static_cast<const uint8_t *>(buf);
    size_t sent = 0;
    while (sent < n) {
        if (stop_) {
            return false;
        }
#if defined(_WIN32)
        const size_t remaining = n - sent;
        const int chunk = static_cast<int>(std::min(
            remaining, static_cast<size_t>(std::numeric_limits<int>::max())));
        const int w = ::send(native_socket(fd), reinterpret_cast<const char *>(p + sent), chunk, 0);
        if (w == SOCKET_ERROR) {
            const int socket_error = ::WSAGetLastError();
            if (socket_error == WSAEINTR) {
                continue;
            }
            if (socket_error == WSAEWOULDBLOCK) {
                WSAPOLLFD pfd{};
                pfd.fd = native_socket(fd);
                pfd.events = POLLWRNORM;
                const int pr = ::WSAPoll(&pfd, 1, 50);
                if (pr == SOCKET_ERROR) {
                    if (::WSAGetLastError() == WSAEINTR) {
                        continue;
                    }
                    return false;
                }
                if (pr == 0) {
                    continue;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    return false;
                }
                continue;
            }
            return false;
        }
#else
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
#endif
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

bool DebugProtocolServer::send_frame(DebugSocketHandle fd, uint32_t type, uint32_t seq,
                                     const void *payload, uint32_t length) {
    FrameHeader hdr{type, seq, length};
    if (!write_full(fd, &hdr, sizeof(hdr))) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    return write_full(fd, payload, length);
}

bool DebugProtocolServer::send_error(DebugSocketHandle fd, uint32_t seq, uint32_t code,
                                     const char *message) {
    const size_t msg_len = message ? std::strlen(message) : 0;
    std::vector<uint8_t> payload(4 + msg_len);
    std::memcpy(payload.data(), &code, 4);
    if (msg_len) {
        std::memcpy(payload.data() + 4, message, msg_len);
    }
    return send_frame(fd, kTypeError, seq, payload.data(), static_cast<uint32_t>(payload.size()));
}

bool DebugProtocolServer::reject(DebugSocketHandle fd, uint32_t seq, uint32_t code,
                                 const char *message) {
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

void DebugProtocolServer::serve_client(DebugSocketHandle client_fd) {
#if defined(_WIN32)
    u_long nonblocking = 1;
    ::ioctlsocket(native_socket(client_fd), FIONBIO, &nonblocking);
#elif GS2_DEBUG_PROTO_UNIX
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

#if defined(_WIN32)
        WSAPOLLFD pfd{};
        pfd.fd = native_socket(client_fd);
        pfd.events = POLLRDNORM;
        const int pr = ::WSAPoll(&pfd, 1, 50);
        if (pr == SOCKET_ERROR) {
            if (::WSAGetLastError() == WSAEINTR) {
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
        if (!(pfd.revents & POLLRDNORM)) {
            continue;
        }
#elif GS2_DEBUG_PROTO_UNIX
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
        case kTypeGetTrace: {
            if (hdr.length != 8) {
                REJECT(client_fd, hdr.seq, kEBadLength, "GET_TRACE requires 8-byte payload");
            }
            uint32_t ago = 0, count = 0;
            std::memcpy(&ago, payload.data() + 0, 4);
            std::memcpy(&count, payload.data() + 4, 4);
            if (count == 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "GET_TRACE count must be >= 1");
            }
            if (count > kMaxTraceRecords) {
                REJECT(client_fd, hdr.seq, kEBadLength, "GET_TRACE count out of range");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeGetTrace, hdr.seq, ago, count, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() < 8) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad get_trace reply");
            }
            uint32_t returned = 0;
            std::memcpy(&returned, reply.data() + 4, 4);
            if (reply.size() != 8 + static_cast<size_t>(returned) * kTraceEntrySize) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad get_trace reply");
            }
            REPLY_OK(kTypeGetTrace, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
            break;
        }
        case kTypeGetRegs: {
            if (hdr.length != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "GET_REGS requires empty payload");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeGetRegs, hdr.seq, 0, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() != kTraceEntrySize) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad get_regs reply");
            }
            REPLY_OK(kTypeGetRegs, hdr.seq, reply.data(), kTraceEntrySize);
            break;
        }
        case kTypeSetRegs: {
            if (hdr.length != kSetRegsPayloadSize) {
                REJECT(client_fd, hdr.seq, kEBadLength, "SET_REGS requires 24-byte payload");
            }
            uint32_t mask = 0;
            std::memcpy(&mask, payload.data() + 0, 4);
            if ((mask & ~kRegMaskAll) != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "SET_REGS unknown mask bits");
            }
            if ((mask & kRegE) && payload[19] > 1) {
                REJECT(client_fd, hdr.seq, kEBadLength, "SET_REGS e must be 0 or 1");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeSetRegs, hdr.seq, 0, 0, 0, payload, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad set_regs reply");
            }
            REPLY_OK(kTypeSetRegs, hdr.seq, nullptr, 0);
            break;
        }
        case kTypeFindMem: {
            if (hdr.length < kFindMemHeaderSize) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM payload too short");
            }
            uint32_t domain = 0, address = 0, length = 0, max_hits = 0, pattern_len = 0, flags = 0;
            std::memcpy(&domain, payload.data() + 0, 4);
            std::memcpy(&address, payload.data() + 4, 4);
            std::memcpy(&length, payload.data() + 8, 4);
            std::memcpy(&max_hits, payload.data() + 12, 4);
            std::memcpy(&pattern_len, payload.data() + 16, 4);
            std::memcpy(&flags, payload.data() + 20, 4);
            if ((flags & ~kFindMemHasMask) != 0) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM unknown flags");
            }
            if (length == 0 || length > kMaxReadMem) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM length out of range");
            }
            if (address > std::numeric_limits<uint32_t>::max() - length) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM address wrap");
            }
            if (pattern_len == 0 || pattern_len > kMaxFindPattern || pattern_len > length) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM pattern_len out of range");
            }
            if (max_hits == 0 || max_hits > kMaxFindHits) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM max_hits out of range");
            }
            if (domain != kMemMain && domain != kMemMegaII && domain != kMemEnsoniq
                && domain != kMemAdbMicro && domain != kMemMainRaw && domain != kMemMegaIIRaw) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM invalid domain");
            }
            const bool has_mask = (flags & kFindMemHasMask) != 0;
            const uint32_t expect = kFindMemHeaderSize + pattern_len + (has_mask ? pattern_len : 0);
            if (hdr.length != expect) {
                REJECT(client_fd, hdr.seq, kEBadLength, "FINDMEM payload length mismatch");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeFindMem, hdr.seq, 0, 0, 0, payload, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err, domain));
            }
            if (reply.size() < 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad findmem reply");
            }
            uint32_t hit_count = 0;
            std::memcpy(&hit_count, reply.data(), 4);
            if (hit_count > max_hits || reply.size() != 4 + static_cast<size_t>(hit_count) * 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad findmem reply");
            }
            REPLY_OK(kTypeFindMem, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
            break;
        }
        case kTypeStateGet: {
            if (hdr.length != 4) {
                REJECT(client_fd, hdr.seq, kEBadLength, "STATE_GET requires 4-byte payload");
            }
            uint32_t device_id_u = 0;
            std::memcpy(&device_id_u, payload.data(), 4);
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeStateGet, hdr.seq, device_id_u, 0, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            REPLY_OK(kTypeStateGet, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
            break;
        }
        case kTypeStateSet: {
            if (hdr.length < 4) {
                REJECT(client_fd, hdr.seq, kEBadLength, "STATE_SET requires device_id + blob");
            }
            uint32_t device_id_u = 0;
            std::memcpy(&device_id_u, payload.data(), 4);
            std::vector<uint8_t> request_data(payload.begin() + 4, payload.end());
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeStateSet, hdr.seq, device_id_u, 0, 0, request_data, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            REPLY_OK(kTypeStateSet, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
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
        case kTypeVideoText: {
            if (hdr.length != kVideoTextReqSize) {
                REJECT(client_fd, hdr.seq, kEBadLength, "VIDEO_TEXT requires 8-byte payload");
            }
            uint32_t page = 0, mode = 0;
            std::memcpy(&page, payload.data() + 0, 4);
            std::memcpy(&mode, payload.data() + 4, 4);
            if (page > 2) {
                REJECT(client_fd, hdr.seq, kEBadLength, "VIDEO_TEXT invalid page");
            }
            if (mode > 7) {
                REJECT(client_fd, hdr.seq, kEBadLength, "VIDEO_TEXT invalid mode");
            }
            if (mode >= 3) {
                REJECT(client_fd, hdr.seq, kEBadLength, "unsupported video mode");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeVideoText, hdr.seq, page, mode, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() < kVideoTextHeaderSize) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad video_text reply");
            }
            uint32_t cols = 0, rows = 0;
            std::memcpy(&cols, reply.data() + 0, 4);
            std::memcpy(&rows, reply.data() + 4, 4);
            if (rows != text_page::kRows
                || (cols != text_page::kCols40 && cols != text_page::kCols80)
                || reply.size() != kVideoTextHeaderSize + static_cast<size_t>(cols) * rows) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad video_text reply");
            }
            REPLY_OK(kTypeVideoText, hdr.seq, reply.data(), static_cast<uint32_t>(reply.size()));
            break;
        }
        case kTypeMount: {
            if (hdr.length < 8) {
                REJECT(client_fd, hdr.seq, kEBadLength, "MOUNT payload too short");
            }
            uint32_t slot = 0, unit = 0;
            std::memcpy(&slot, payload.data() + 0, 4);
            std::memcpy(&unit, payload.data() + 4, 4);
            if (unit > kMaxMediaUnit) {
                REJECT(client_fd, hdr.seq, kEBadLength, "MOUNT unit out of range");
            }
            const size_t path_len = payload.size() - 8;
            if (path_len == 0) {
                // Still bridge so reply is MEDIA_BAD_PATH status (not ERROR).
            } else if (path_len > kMaxMediaPathLen) {
                REJECT(client_fd, hdr.seq, kEBadLength, "MOUNT path too long");
            }
            std::vector<uint8_t> path_bytes(payload.begin() + 8, payload.end());
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypeMount, hdr.seq, slot, unit, 0, path_bytes, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() != 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad mount reply");
            }
            REPLY_OK(kTypeMount, hdr.seq, reply.data(), 4);
            break;
        }
        case kTypeUnmount: {
            if (hdr.length != 8) {
                REJECT(client_fd, hdr.seq, kEBadLength, "UNMOUNT requires 8-byte payload");
            }
            uint32_t slot = 0, unit = 0;
            std::memcpy(&slot, payload.data() + 0, 4);
            std::memcpy(&unit, payload.data() + 4, 4);
            if (unit > kMaxMediaUnit) {
                REJECT(client_fd, hdr.seq, kEBadLength, "UNMOUNT unit out of range");
            }
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            static const std::vector<uint8_t> kEmptyRequest;
            if (!submit_and_wait(kTypeUnmount, hdr.seq, slot, unit, 0, kEmptyRequest, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (reply.size() != 4) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad unmount reply");
            }
            REPLY_OK(kTypeUnmount, hdr.seq, reply.data(), 4);
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
        case kTypePasteText: {
            std::vector<uint8_t> reply;
            uint32_t err = 0;
            if (!submit_and_wait(kTypePasteText, hdr.seq, 0, 0, 0, payload, reply, err,
                                 kMainThreadTimeoutMs)) {
                return;
            }
            if (err != 0) {
                REJECT(client_fd, hdr.seq, err, bridge_error_message(err));
            }
            if (!reply.empty()) {
                REJECT(client_fd, hdr.seq, kEInternal, "bad paste_text reply");
            }
            REPLY_OK(kTypePasteText, hdr.seq, nullptr, 0);
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
