/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "util/EventQueue.hpp"

// screencap_texture is 910x263; PNG output doubles scanlines for aspect.
#define MAX_SCREENSHOT_WIDTH 910
#define MAX_SCREENSHOT_SRC_HEIGHT 263
#define MAX_SCREENSHOT_HEIGHT (MAX_SCREENSHOT_SRC_HEIGHT * 2)

/** Status message from worker → main. Copied by value into the SPSC ring. */
struct ScreenshotStatusMsg {
    enum Type : uint8_t { NONE = 0, DONE = 1 };
    Type type = NONE;
    char text[192]{};
};

/**
 * SPSC ring (same shape as SerialQueue): worker may only send(), main may only get().
 * Depth 4 is ample for one-pending screenshot jobs.
 */
class ScreenshotStatusQueue {
    constexpr static uint32_t queue_depth = 4;
    constexpr static uint32_t queue_mask = queue_depth - 1;

    ScreenshotStatusMsg queue[queue_depth]{};
    uint32_t head = 0;
    uint32_t tail = 0;

public:
    inline bool is_empty() const { return head == tail; }
    inline bool is_full() const { return ((head + 1) & queue_mask) == tail; }

    inline ScreenshotStatusMsg get() {
        if (is_empty()) {
            return ScreenshotStatusMsg{};
        }
        ScreenshotStatusMsg msg = queue[tail];
        tail = (tail + 1) & queue_mask;
        return msg;
    }

    inline bool send(const ScreenshotStatusMsg &msg) {
        if (is_full()) {
            return false;
        }
        queue[head] = msg;
        head = (head + 1) & queue_mask;
        return true;
    }
};

class ScreenshotWriter {
    uint8_t *buffer_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    std::string path_;
    char display_msg_[256]{}; // main-thread only; pointed at by EventQueue OSD events
    ScreenshotStatusQueue status_q_;
    std::atomic<bool> pending_{false};
    std::atomic<bool> quit_{false};
    SDL_Thread *thread_ = nullptr;
    SDL_Semaphore *sem_ = nullptr;

    static int SDLCALL thread_entry(void *data);
    void worker_loop();

public:
    ScreenshotWriter();
    ~ScreenshotWriter();

    ScreenshotWriter(const ScreenshotWriter &) = delete;
    ScreenshotWriter &operator=(const ScreenshotWriter &) = delete;

    /** Copy surface (with scanline doubling) into the prealloc buffer and wake the worker.
     *  Returns false if a write is already pending or the surface is too large.
     *  Main thread only. */
    bool try_submit(SDL_Surface *surface, const std::string &path);
    bool is_pending() const { return pending_.load(std::memory_order_acquire); }

    /** Drain worker status messages into EventQueue. Main thread only; never blocks. */
    void poll(EventQueue *event_queue);
};
