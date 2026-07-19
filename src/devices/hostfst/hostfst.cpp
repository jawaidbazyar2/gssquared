/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Host FST guest interface (WDM $FF) adapted from Kelvin Sherlock / GSPlus (GPL 2.0).
 */

#include "devices/hostfst/hostfst.hpp"

#include "computer.hpp"
#include "cpu.hpp"
#include "paths.hpp"
#include "util/SystemSettings.hpp"

#include <cstdio>
#include <string>

#if defined(_WIN32) || defined(__EMSCRIPTEN__)

std::string hostfst_resolved_dir() {
    const std::string &configured = SystemSettings::instance().host_fst_dir();
    if (!configured.empty()) {
        return configured;
    }
    return Paths::documents_folder();
}

void hostfst_apply_dir(const std::string &path) {
    SystemSettings::instance().set_host_fst_dir(path);
}

void init_hostfst(computer_t * /*computer*/, SlotType_t /*slot*/) {
    fprintf(stderr, "Host FST: not available on this platform\n");
}

#else

#include "devices/hostfst/hostfst_engine.hpp"
#include "devices/hostfst/defc_shim.h"
#include "devices/hostfst/host_common.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstring>

namespace {

constexpr int kRequestRingDepth = 8;
constexpr int kReplyRingDepth = 8;

enum class HostFstMsg : uint8_t {
    None = 0,
    RunCall,
    Remount,
    Shutdown,
};

struct HostFstRequest {
    HostFstMsg msg = HostFstMsg::None;
};

struct HostFstReply {
    HostFstMsg msg = HostFstMsg::None;
};

template <typename T, int Depth>
class SpscRing {
    static_assert((Depth & (Depth - 1)) == 0, "Depth must be power of two");
    T slots_[Depth]{};
    std::atomic<uint32_t> head_{0};
    std::atomic<uint32_t> tail_{0};

public:
    bool send(const T &item) {
        const uint32_t h = head_.load(std::memory_order_relaxed);
        const uint32_t next = (h + 1) & (Depth - 1);
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;
        }
        slots_[h] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool get(T *out) {
        const uint32_t t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) {
            return false;
        }
        *out = slots_[t];
        tail_.store((t + 1) & (Depth - 1), std::memory_order_release);
        return true;
    }
};

struct hostfst_state_t {
    computer_t *computer = nullptr;
    SDL_Thread *worker = nullptr;
    SDL_Semaphore *wake = nullptr;
    SDL_Semaphore *done = nullptr;
    SpscRing<HostFstRequest, kRequestRingDepth> requests;
    SpscRing<HostFstReply, kReplyRingDepth> replies;
    std::atomic<bool> running{false};
};

hostfst_state_t *g_hostfst = nullptr;

std::string resolve_host_dir() {
    const std::string &configured = SystemSettings::instance().host_fst_dir();
    if (!configured.empty()) {
        return configured;
    }
    return Paths::documents_folder();
}

void apply_resolved_path_to_cfg(bool log = false) {
    const std::string dir = resolve_host_dir();
    hostfst_set_host_path(dir.c_str());
    if (log) {
        fprintf(stderr, "Host FST: host directory = %s\n", dir.c_str());
    }
}

int hostfst_worker_main(void *userdata) {
    auto *st = static_cast<hostfst_state_t *>(userdata);
    while (st->running.load(std::memory_order_acquire)) {
        SDL_WaitSemaphore(st->wake);
        HostFstRequest req;
        while (st->requests.get(&req)) {
            if (req.msg == HostFstMsg::Shutdown) {
                HostFstReply reply{HostFstMsg::Shutdown};
                st->replies.send(reply);
                SDL_SignalSemaphore(st->done);
                return 0;
            }
            if (req.msg == HostFstMsg::RunCall) {
                host_fst();
                HostFstReply reply{HostFstMsg::RunCall};
                st->replies.send(reply);
                SDL_SignalSemaphore(st->done);
            }
            if (req.msg == HostFstMsg::Remount) {
                host_fst_remount();
                HostFstReply reply{HostFstMsg::Remount};
                st->replies.send(reply);
                SDL_SignalSemaphore(st->done);
            }
        }
    }
    return 0;
}

bool hostfst_submit(hostfst_state_t *st, HostFstMsg msg) {
    HostFstRequest req{msg};
    if (!st->requests.send(req)) {
        fprintf(stderr, "Host FST: request ring full\n");
        return false;
    }
    SDL_SignalSemaphore(st->wake);
    SDL_WaitSemaphore(st->done);
    HostFstReply reply;
    while (st->replies.get(&reply)) {
        /* drain */
    }
    return true;
}

void hostfst_wdm(cpu_state *cpu, void *context) {
    auto *st = static_cast<hostfst_state_t *>(context);
    if (st == nullptr || cpu == nullptr) {
        return;
    }

    apply_resolved_path_to_cfg(false);
    hostfst_bind_cpu(cpu);
    hostfst_sync_engine_from_cpu(cpu);

    if (!hostfst_submit(st, HostFstMsg::RunCall)) {
        return;
    }

    hostfst_sync_cpu_from_engine(cpu);
}

}  // namespace

std::string hostfst_resolved_dir() {
    return resolve_host_dir();
}

void hostfst_apply_dir(const std::string &path) {
    SystemSettings::instance().set_host_fst_dir(path);
    apply_resolved_path_to_cfg(true);
    // Previous code only host_shutdown()'d, leaving host_root NULL so every
    // subsequent WDM call returned networkError until GS/OS reloaded the FST.
    if (g_hostfst != nullptr) {
        hostfst_submit(g_hostfst, HostFstMsg::Remount);
    } else {
        host_fst_remount();
    }
}

void init_hostfst(computer_t *computer, SlotType_t /*slot*/) {
    auto *st = new hostfst_state_t();
    st->computer = computer;
    st->wake = SDL_CreateSemaphore(0);
    st->done = SDL_CreateSemaphore(0);
    st->running.store(true, std::memory_order_release);
    st->worker = SDL_CreateThread(hostfst_worker_main, "hostfst", st);
    if (st->worker == nullptr) {
        fprintf(stderr, "Host FST: failed to create worker thread: %s\n", SDL_GetError());
        SDL_DestroySemaphore(st->wake);
        SDL_DestroySemaphore(st->done);
        delete st;
        return;
    }

    g_hostfst = st;
    apply_resolved_path_to_cfg(true);

    computer->cpu->set_wdm_handler(0xFF, {hostfst_wdm, st});

    computer->register_shutdown_handler([st]() {
        st->running.store(false, std::memory_order_release);
        HostFstRequest req{HostFstMsg::Shutdown};
        st->requests.send(req);
        SDL_SignalSemaphore(st->wake);
        SDL_WaitThread(st->worker, nullptr);
        host_shutdown();
        SDL_DestroySemaphore(st->wake);
        SDL_DestroySemaphore(st->done);
        if (g_hostfst == st) {
            g_hostfst = nullptr;
        }
        delete st;
        return true;
    });

    fprintf(stderr, "Host FST: registered WDM $FF handler\n");
}

#endif
