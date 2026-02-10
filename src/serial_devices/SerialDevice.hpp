#pragma once

#include <cstdint>
#include <SDL3/SDL.h>

enum serial_message_type_t {
    MESSAGE_NONE = 0,
    MESSAGE_DATA,
    MESSAGE_BREAK_ON,
    MESSAGE_BREAK_OFF,
    MESSAGE_SHUTDOWN,
};

struct SerialMessage {
    serial_message_type_t type;
    uint64_t data;
};

class SerialQueue {
    constexpr static uint32_t queue_depth = 128; // must be a power of 2!!
    constexpr static uint32_t queue_mask = queue_depth - 1;
    
    SerialMessage queue[queue_depth];
    uint32_t head = 0;
    uint32_t tail = 0;

    public:
        SerialQueue() = default;
        ~SerialQueue() = default;
        
        inline bool is_empty() { return head == tail; }
        inline SerialMessage get() { 
            if (is_empty()) {
                return SerialMessage{MESSAGE_NONE, 0};
            }
            SerialMessage msg = queue[tail];
            tail = (tail + 1) & queue_mask;
            return msg;
        }

        inline bool is_full() { return ((head + 1) & queue_mask) == tail; }
        inline bool send(SerialMessage msg) {
            if (is_full()) {
                return false;
            }
            queue[head] = msg;
            head = (head + 1) & queue_mask;
            return true;
        }

        inline uint64_t get_count() { return (head - tail) & queue_mask; }
};

class SerialDevice {
    protected:
        const char *name;
        SDL_Thread *thread;

    public:
        SerialQueue q_host; // host -> dev queue
        SerialQueue q_dev;  // dev -> host queue

        SerialDevice(const char *name);
        virtual ~SerialDevice();

        /*
           This method only exits when it receives a SHUTDOWN message. Otherwise
           processes in a loop forever.
           Must ONLY q_host->get() and q_dev->send() to prevent race conditions.
        */

        const char *get_name() { return name; }
        virtual void device_loop() = 0;
};
