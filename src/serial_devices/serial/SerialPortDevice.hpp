/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

#include "serial_devices/SerialDevice.hpp"
#include "serial_devices/host/HostSerial.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "util/Event.hpp"
#include "util/EventQueue.hpp"

struct SerialPortStatusMsg {
    enum Type : uint8_t { NONE = 0, INFO = 1 };
    Type type = NONE;
    char text[192]{};
};

class SerialPortStatusQueue {
    constexpr static uint32_t queue_depth = 4;
    constexpr static uint32_t queue_mask = queue_depth - 1;

    SerialPortStatusMsg queue[queue_depth]{};
    uint32_t head = 0;
    uint32_t tail = 0;

public:
    inline bool is_empty() const { return head == tail; }
    inline bool is_full() const { return ((head + 1) & queue_mask) == tail; }

    inline SerialPortStatusMsg get() {
        if (is_empty()) {
            return SerialPortStatusMsg{};
        }
        SerialPortStatusMsg msg = queue[tail];
        tail = (tail + 1) & queue_mask;
        return msg;
    }

    inline bool send(const SerialPortStatusMsg &msg) {
        if (is_full()) {
            return false;
        }
        queue[head] = msg;
        head = (head + 1) & queue_mask;
        return true;
    }
};

class SerialPortDevice : public SerialDevice {
    std::string path_;
    HostSerial port_;
    host_serial_line_t line_{};
    bool have_line_ = false;
    bool fail_toasted_ = false;
    uint32_t last_try_ms_ = 0;
    char display_msg_[256]{};
    SerialPortStatusQueue status_q_;
    EventQueue *event_queue_ = nullptr;

    void notify(const char *fmt, const char *arg) {
        SerialPortStatusMsg msg;
        msg.type = SerialPortStatusMsg::INFO;
        std::snprintf(msg.text, sizeof(msg.text), fmt, arg);
        status_q_.send(msg);
    }

    void try_attach() {
        if (port_.is_attached() || path_.empty()) {
            return;
        }
        const uint32_t now = SDL_GetTicks();
        if (last_try_ms_ != 0 && (now - last_try_ms_) < 1000) {
            return;
        }
        last_try_ms_ = now;
        if (port_.attach(path_.c_str())) {
            if (have_line_) {
                port_.configure(line_);
            }
            notify("Serial attached %s", path_.c_str());
            fail_toasted_ = false;
            return;
        }
        if (!fail_toasted_) {
            notify("Serial open failed %s", path_.c_str());
            fail_toasted_ = true;
        }
    }

    void handle_io_error() {
        if (port_.is_attached()) {
            port_.detach();
        }
        notify("Serial detached %s", path_.c_str());
        fail_toasted_ = false;
        last_try_ms_ = SDL_GetTicks();
    }

    void poll() {
        if (!event_queue_) {
            return;
        }
        SerialPortStatusMsg msg = status_q_.get();
        if (msg.type == SerialPortStatusMsg::NONE) {
            return;
        }
        std::snprintf(display_msg_, sizeof(display_msg_), "%s", msg.text);
        event_queue_->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, display_msg_));
    }

public:
    SerialPortDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames,
                     const char *port_id, const std::string &path)
        : SerialDevice("SerialPortDevice", port_id),
          path_(path),
          event_queue_(event_queue) {
        if (frames) {
            frames->registerHandler([this]() {
                poll();
                return true;
            });
        }
    }

    ~SerialPortDevice() {
        if (thread) {
            SDL_Log("SerialDevice: %s shutting down", this->name);
            SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
            q_host.send(msg);
            SDL_WaitThread(thread, NULL);
            thread = nullptr;
        }
        port_.detach();
        event_queue_ = nullptr;
    }

    void device_loop() override {
        while (true) {
            SDL_Delay(5);
            try_attach();

            while (!q_host.is_empty()) {
                SerialMessage msg = q_host.get();
                switch (msg.type) {
                    case MESSAGE_SHUTDOWN:
                        port_.detach();
                        return;
                    case MESSAGE_LINE:
                        line_ = host_serial_unpack_line(msg.data);
                        have_line_ = true;
                        if (port_.is_attached()) {
                            port_.configure(line_);
                        }
                        break;
                    case MESSAGE_DATA: {
                        if (!port_.is_attached()) {
                            break;
                        }
                        const uint8_t byte = static_cast<uint8_t>(msg.data);
                        const int n = port_.send(&byte, 1);
                        if (n < 0) {
                            handle_io_error();
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            if (!port_.is_attached()) {
                continue;
            }
            uint8_t buf[32];
            const int n = port_.receive(buf, static_cast<int>(sizeof(buf)));
            if (n < 0) {
                handle_io_error();
                continue;
            }
            for (int i = 0; i < n; i++) {
                q_dev.send(SerialMessage{MESSAGE_DATA, buf[i]});
            }
        }
    }
};
