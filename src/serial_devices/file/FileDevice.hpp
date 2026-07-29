#pragma once

#include <SDL3/SDL.h>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cstring>

#include "serial_devices/SerialDevice.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "util/Event.hpp"
#include "util/EventQueue.hpp"

/** Status message from FileDevice worker → main. Copied by value into the SPSC ring. */
struct FileCloseStatusMsg {
    enum Type : uint8_t { NONE = 0, CLOSED = 1 };
    Type type = NONE;
    char text[192]{};
};

/**
 * SPSC ring (same shape as SerialQueue / ScreenshotStatusQueue):
 * worker may only send(), main may only get().
 */
class FileCloseStatusQueue {
    constexpr static uint32_t queue_depth = 4;
    constexpr static uint32_t queue_mask = queue_depth - 1;

    FileCloseStatusMsg queue[queue_depth]{};
    uint32_t head = 0;
    uint32_t tail = 0;

public:
    inline bool is_empty() const { return head == tail; }
    inline bool is_full() const { return ((head + 1) & queue_mask) == tail; }

    inline FileCloseStatusMsg get() {
        if (is_empty()) {
            return FileCloseStatusMsg{};
        }
        FileCloseStatusMsg msg = queue[tail];
        tail = (tail + 1) & queue_mask;
        return msg;
    }

    inline bool send(const FileCloseStatusMsg &msg) {
        if (is_full()) {
            return false;
        }
        queue[head] = msg;
        head = (head + 1) & queue_mask;
        return true;
    }
};

class FileDevice : public SerialDevice {
    private:
        FILE *file;
        uint64_t last_write_ticks = 0;
        static constexpr uint64_t idle_close_ms = 10'000;
        char filename[256]{};
        char display_msg_[256]{}; // main-thread only; pointed at by EventQueue OSD events
        FileCloseStatusQueue status_q_;
        EventQueue *event_queue_ = nullptr;

        void notify_closed() {
            printf("file %s closed\n", filename);
            FileCloseStatusMsg msg;
            msg.type = FileCloseStatusMsg::CLOSED;
            std::snprintf(msg.text, sizeof(msg.text), "File closed %s", filename);
            // Worker → main: SPSC ring only. Never touch EventQueue here.
            status_q_.send(msg);
        }

        void mark_write() {
            last_write_ticks = SDL_GetTicks();
        }

        /** Drain worker close notifications into EventQueue. Main thread only; never blocks. */
        void poll() {
            if (!event_queue_) {
                return;
            }
            // At most one per frame: EventQueue stores a pointer into display_msg_, and
            // frame_appevent consumes a single event per call.
            FileCloseStatusMsg msg = status_q_.get();
            if (msg.type == FileCloseStatusMsg::NONE) {
                return;
            }
            std::snprintf(display_msg_, sizeof(display_msg_), "%s", msg.text);
            event_queue_->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, display_msg_));
        }

    public:
        FileDevice(EventQueue *event_queue, DeviceFrameDispatcher *frames, const char *port_id)
            : SerialDevice("FileDevice", port_id), event_queue_(event_queue) {
            file = NULL;
            if (frames) {
                frames->registerHandler([this]() {
                    poll();
                    return true;
                });
            }
        }

        ~FileDevice() {
            // Ensure thread stops before our members are destroyed
            if (thread) {
                SDL_Log("SerialDevice: %s shutting down", this->name);
                SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
                q_host.send(msg);
                SDL_WaitThread(thread, NULL);
                thread = nullptr;
            }
            close_file(false); // no toast on teardown; console log still useful if a file was open
            event_queue_ = nullptr;
        }

        void open_file() {
            // construct file name from "GS2.{Extension}.{YYYYMMDDHHMMSS}"
            time_t now = time(NULL);
            struct tm *tm_now = localtime(&now);
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_now);

            snprintf(filename, sizeof(filename), "GS2.%s.%s", port_id, timestamp);
            printf("FileDevice: opening file %s\n", filename);

            file = fopen(filename, "w");
            if (file == NULL) {
                printf("Failed to open file %s\n", filename);
                filename[0] = '\0';
                return;
            }
            mark_write();
        }

        void close_file(bool notify = true) {
            if (file != NULL) {
                fclose(file);
                file = NULL;
                last_write_ticks = 0;
                if (filename[0] != '\0') {
                    if (notify) {
                        notify_closed();
                    } else {
                        printf("file %s closed\n", filename);
                    }
                    filename[0] = '\0';
                }
            }
        }

        void device_loop() override {
            while (true) {
                SDL_Delay(10);

                if (file && last_write_ticks != 0 &&
                    (SDL_GetTicks() - last_write_ticks) >= idle_close_ms) {
                    close_file();
                }

                while (!q_host.is_empty()) {
                    SerialMessage msg = q_host.get();
                    switch (msg.type) {
                        case MESSAGE_SHUTDOWN:
                            return;
                        case MESSAGE_CLOSE:
                            close_file();
                            break;
                        case MESSAGE_DATA:
                            if (!file) open_file();
                            if (file) {
                                fwrite(&msg.data, 1, 1, file);
                                mark_write();
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
};
