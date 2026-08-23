#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_clipboard.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "serial_devices/SerialDevice.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "util/Event.hpp"
#include "util/EventQueue.hpp"

/** Status message from ClipboardDevice worker → main. Copied by value into the SPSC ring. */
struct ClipboardCloseStatusMsg {
    enum Type : uint8_t { NONE = 0, CLOSED = 1 };
    Type type = NONE;
    uint32_t len = 0;
    bool truncated = false;
};

/**
 * SPSC ring (same shape as SerialQueue / FileCloseStatusQueue):
 * worker may only send(), main may only get().
 */
class ClipboardCloseStatusQueue {
    constexpr static uint32_t queue_depth = 4;
    constexpr static uint32_t queue_mask = queue_depth - 1;

    ClipboardCloseStatusMsg queue[queue_depth]{};
    uint32_t head = 0;
    uint32_t tail = 0;

public:
    inline bool is_empty() const { return head == tail; }
    inline bool is_full() const { return ((head + 1) & queue_mask) == tail; }

    inline ClipboardCloseStatusMsg get() {
        if (is_empty()) {
            return ClipboardCloseStatusMsg{};
        }
        ClipboardCloseStatusMsg msg = queue[tail];
        tail = (tail + 1) & queue_mask;
        return msg;
    }

    inline bool send(const ClipboardCloseStatusMsg &msg) {
        if (is_full()) {
            return false;
        }
        queue[head] = msg;
        head = (head + 1) & queue_mask;
        return true;
    }
};

class ClipboardDevice : public SerialDevice {
    private:
        static constexpr size_t kBufSize = 128 * 1024;
        static constexpr uint64_t idle_close_ms = 10'000;

        char capture_[kBufSize]{};
        size_t len_ = 0;
        bool truncated_ = false;
        bool open_ = false;
        uint8_t last_eol_ = 0; // last CR/LF seen (after hi-bit strip); collapses CRLF / LFCR
        uint64_t last_write_ticks = 0;

        // Worker publishes here on close; main reads only after CLOSED.
        char pending_[kBufSize + 1]{};
        uint32_t pending_len_ = 0;
        bool pending_truncated_ = false;

        char display_msg_[256]{}; // main-thread only; pointed at by EventQueue OSD events
        ClipboardCloseStatusQueue status_q_;
        EventQueue *event_queue_ = nullptr;

        void mark_write() {
            last_write_ticks = SDL_GetTicks();
        }

        void open_capture() {
            len_ = 0;
            truncated_ = false;
            last_eol_ = 0;
            open_ = true;
            mark_write();
        }

        void emit_byte(uint8_t c) {
            if (len_ >= kBufSize) {
                truncated_ = true;
                mark_write();
                return;
            }
            capture_[len_++] = static_cast<char>(c);
            mark_write();
        }

        void append_byte(uint8_t raw) {
            uint8_t c = static_cast<uint8_t>(raw & 0x7F);
            if (c == 0) {
                return; // 0x00 / 0x80 would truncate a C-string
            }
            // IIgs Slot 1 printer port defaults to "Add LF after CR: YES", so
            // the guest often sends CRLF (or LFCR). Collapse the pair to one LF.
            // Bare CR or LF still becomes LF; CR CR / LF LF stay as blank lines.
            if (c == 0x0D || c == 0x0A) {
                if ((c == 0x0A && last_eol_ == 0x0D) || (c == 0x0D && last_eol_ == 0x0A)) {
                    last_eol_ = 0;
                    mark_write();
                    return;
                }
                last_eol_ = c;
                emit_byte(0x0A);
                return;
            }
            last_eol_ = 0;
            emit_byte(c);
        }

        void close_capture(bool notify = true) {
            if (!open_) {
                return;
            }
            open_ = false;
            last_write_ticks = 0;
            if (notify && len_ > 0) {
                std::memcpy(pending_, capture_, len_);
                pending_[len_] = '\0';
                pending_len_ = static_cast<uint32_t>(len_);
                pending_truncated_ = truncated_;

                ClipboardCloseStatusMsg msg;
                msg.type = ClipboardCloseStatusMsg::CLOSED;
                msg.len = pending_len_;
                msg.truncated = pending_truncated_;
                // Worker → main: SPSC ring only. Never touch EventQueue or SDL clipboard here.
                status_q_.send(msg);
                printf("ClipboardDevice: closed %u bytes%s\n",
                       pending_len_, pending_truncated_ ? " (truncated)" : "");
            }
            len_ = 0;
            truncated_ = false;
            last_eol_ = 0;
        }

        /** Drain worker close notifications. Main thread only; never blocks. */
        void poll() {
            if (!event_queue_) {
                return;
            }
            ClipboardCloseStatusMsg msg = status_q_.get();
            if (msg.type == ClipboardCloseStatusMsg::NONE) {
                return;
            }
            if (!SDL_SetClipboardText(pending_)) {
                printf("ClipboardDevice: SDL_SetClipboardText failed: %s\n", SDL_GetError());
            }
            if (msg.truncated) {
                std::snprintf(display_msg_, sizeof(display_msg_),
                              "Copied %u bytes to clipboard (truncated)", msg.len);
            } else {
                std::snprintf(display_msg_, sizeof(display_msg_),
                              "Copied %u bytes to clipboard", msg.len);
            }
            event_queue_->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, display_msg_));
        }

    public:
        ClipboardDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames, const char *port_id)
            : SerialDevice("ClipboardDevice", port_id), event_queue_(event_queue) {
            if (frames) {
                frames->registerHandler([this]() {
                    poll();
                    return true;
                });
            }
        }

        ~ClipboardDevice() {
            if (thread) {
                SDL_Log("SerialDevice: %s shutting down", this->name);
                SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
                q_host.send(msg);
                SDL_WaitThread(thread, NULL);
                thread = nullptr;
            }
            close_capture(false); // do not steal the host clipboard on emulator quit
            event_queue_ = nullptr;
        }

        void device_loop() override {
            while (true) {
                SDL_Delay(10);

                if (open_ && last_write_ticks != 0 &&
                    (SDL_GetTicks() - last_write_ticks) >= idle_close_ms) {
                    close_capture();
                }

                while (!q_host.is_empty()) {
                    SerialMessage msg = q_host.get();
                    switch (msg.type) {
                        case MESSAGE_SHUTDOWN:
                            return;
                        case MESSAGE_CLOSE:
                            close_capture();
                            break;
                        case MESSAGE_DATA:
                            if (!open_) {
                                open_capture();
                            }
                            append_byte(static_cast<uint8_t>(msg.data));
                            break;
                        default:
                            break;
                    }
                }
            }
        }
};
