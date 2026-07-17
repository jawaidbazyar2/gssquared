/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "ScreenshotWriter.hpp"

#include <cstdio>
#include <cstring>

#include <SDL3_image/SDL_image.h>

#include "devices/displaypp/RGBA.hpp"
#include "util/Event.hpp"

int SDLCALL ScreenshotWriter::thread_entry(void *data) {
    static_cast<ScreenshotWriter *>(data)->worker_loop();
    return 0;
}

ScreenshotWriter::ScreenshotWriter() {
    const size_t buf_size = static_cast<size_t>(MAX_SCREENSHOT_WIDTH) * MAX_SCREENSHOT_HEIGHT * 4;
    buffer_ = new uint8_t[buf_size];
    sem_ = SDL_CreateSemaphore(0);
    thread_ = SDL_CreateThread(thread_entry, "gs2-screenshot", this);
    if (!thread_) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "ScreenshotWriter: SDL_CreateThread failed: %s", SDL_GetError());
    }
}

ScreenshotWriter::~ScreenshotWriter() {
    quit_.store(true, std::memory_order_release);
    if (sem_) {
        SDL_SignalSemaphore(sem_);
    }
    if (thread_) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
    if (sem_) {
        SDL_DestroySemaphore(sem_);
        sem_ = nullptr;
    }
    delete[] buffer_;
    buffer_ = nullptr;
}

bool ScreenshotWriter::try_submit(SDL_Surface *surface, const std::string &path) {
    if (!surface || !buffer_ || !sem_ || !thread_) {
        return false;
    }

    bool expected = false;
    if (!pending_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return false;
    }

    SDL_Surface *converted = SDL_ConvertSurface(surface, PIXEL_FORMAT);
    if (!converted) {
        pending_.store(false, std::memory_order_release);
        return false;
    }

    const int w = converted->w;
    const int h = converted->h;
    if (w <= 0 || h <= 0 || w > MAX_SCREENSHOT_WIDTH || h > MAX_SCREENSHOT_SRC_HEIGHT) {
        SDL_DestroySurface(converted);
        pending_.store(false, std::memory_order_release);
        return false;
    }

    const int bpp = 4;
    const int src_row_bytes = w * bpp;
    uint8_t *dst = buffer_;
    for (int y = 0; y < h; y++) {
        const uint8_t *src = static_cast<const uint8_t *>(converted->pixels) + y * converted->pitch;
        std::memcpy(dst, src, static_cast<size_t>(src_row_bytes));
        dst += src_row_bytes;
        std::memcpy(dst, src, static_cast<size_t>(src_row_bytes));
        dst += src_row_bytes;
    }
    SDL_DestroySurface(converted);

    width_ = w;
    height_ = h * 2;
    path_ = path;
    SDL_SignalSemaphore(sem_);
    return true;
}

void ScreenshotWriter::poll(EventQueue *event_queue) {
    if (!event_queue) {
        return;
    }
    // At most one per frame: EventQueue stores a pointer into display_msg_, and
    // frame_appevent consumes a single event per call.
    ScreenshotStatusMsg msg = status_q_.get();
    if (msg.type == ScreenshotStatusMsg::NONE) {
        return;
    }
    std::snprintf(display_msg_, sizeof(display_msg_), "%s", msg.text);
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, display_msg_));
}

void ScreenshotWriter::worker_loop() {
    while (true) {
        SDL_WaitSemaphore(sem_);
        if (quit_.load(std::memory_order_acquire)) {
            break;
        }

        SDL_Surface *surf = SDL_CreateSurfaceFrom(
            width_, height_, PIXEL_FORMAT, buffer_, width_ * 4);
        bool ok = false;
        if (surf) {
            ok = IMG_SavePNG(surf, path_.c_str());
            SDL_DestroySurface(surf);
        }

        ScreenshotStatusMsg msg;
        msg.type = ScreenshotStatusMsg::DONE;
        if (ok) {
            const char *base = path_.c_str();
            const char *slash = std::strrchr(base, '/');
#ifdef _WIN32
            const char *bslash = std::strrchr(base, '\\');
            if (bslash && (!slash || bslash > slash)) {
                slash = bslash;
            }
#endif
            if (slash && slash[1]) {
                base = slash + 1;
            }
            std::snprintf(msg.text, sizeof(msg.text), "Saved %s", base);
        } else {
            std::snprintf(msg.text, sizeof(msg.text), "Screenshot save failed");
        }
        // Worker → main: SPSC ring only. Never touch EventQueue here.
        status_q_.send(msg);
        pending_.store(false, std::memory_order_release);
    }
}
