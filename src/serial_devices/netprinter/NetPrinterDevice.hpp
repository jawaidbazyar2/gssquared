/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "serial_devices/SerialDevice.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "util/Event.hpp"
#include "util/EventQueue.hpp"

/** Status message from NetPrinterDevice worker → main. Copied by value into the SPSC ring. */
struct NetPrinterStatusMsg {
    enum Type : uint8_t { NONE = 0, TOAST = 1 };
    Type type = NONE;
    char text[192]{};
};

class NetPrinterStatusQueue {
    constexpr static uint32_t queue_depth = 4;
    constexpr static uint32_t queue_mask = queue_depth - 1;

    NetPrinterStatusMsg queue[queue_depth]{};
    uint32_t head = 0;
    uint32_t tail = 0;

public:
    inline bool is_empty() const { return head == tail; }
    inline bool is_full() const { return ((head + 1) & queue_mask) == tail; }

    inline NetPrinterStatusMsg get() {
        if (is_empty()) {
            return NetPrinterStatusMsg{};
        }
        NetPrinterStatusMsg msg = queue[tail];
        tail = (tail + 1) & queue_mask;
        return msg;
    }

    inline bool send(const NetPrinterStatusMsg &msg) {
        if (is_full()) {
            return false;
        }
        queue[head] = msg;
        head = (head + 1) & queue_mask;
        return true;
    }
};

class NetPrinterDevice : public SerialDevice {
    std::string host_;
    int tcp_port_ = 0;
    EventQueue *event_queue_ = nullptr;
    DeviceFrameDispatcher *frames_ = nullptr;
    size_t frame_handler_id_ = static_cast<size_t>(-1);
    NetPrinterStatusQueue status_q_;
    char display_msg_[256]{};
    std::atomic<bool> socket_open_{false};

    struct Impl;
    std::unique_ptr<Impl> impl_;

    void poll() {
        if (!event_queue_) {
            return;
        }
        NetPrinterStatusMsg msg = status_q_.get();
        if (msg.type == NetPrinterStatusMsg::NONE) {
            return;
        }
        std::snprintf(display_msg_, sizeof(display_msg_), "%s", msg.text);
        event_queue_->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, display_msg_));
    }

public:
    NetPrinterDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames, const char *port_id,
                     const std::string &host, int tcp_port);
    ~NetPrinterDevice() override;

    void device_loop() override;
    void format_hud_status(char *buf, size_t n) const override;

    void post_toast(const char *text) {
        NetPrinterStatusMsg msg;
        msg.type = NetPrinterStatusMsg::TOAST;
        std::snprintf(msg.text, sizeof(msg.text), "%s", text);
        status_q_.send(msg);
    }

    const std::string &host() const { return host_; }
    int tcp_port() const { return tcp_port_; }
    bool is_socket_open() const { return socket_open_.load(std::memory_order_relaxed); }
    void set_socket_open(bool open) { socket_open_.store(open, std::memory_order_relaxed); }
};
