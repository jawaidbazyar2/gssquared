#pragma once

#include <cstdint>

enum message_type_t {
    MESSAGE_NONE = 0,
    MESSAGE_DATA,
    MESSAGE_BREAK_ON,
    MESSAGE_BREAK_OFF,
};

struct Message {
    message_type_t type;
    uint64_t data;
};

class SerialQueue {
    Message queue[128];
    uint32_t queue_depth = 128;
    uint32_t head = 0;
    uint32_t tail = 0;

    public:
        SerialQueue() = default;
        ~SerialQueue() = default;
        
        inline bool is_empty() { return head != tail; }
        inline Message get() { 
            if (is_empty()) {
                return Message{MESSAGE_NONE, 0};
            }
            Message msg = queue[tail];
            tail = (tail + 1) % queue_depth;
            return msg;
        }

        inline bool is_full() { return (head + 1) % queue_depth == tail; }
        inline bool send(Message msg) {
            if (is_full()) {
                return false;
            }
            queue[head] = msg;
            head = (head + 1) % queue_depth;
            return true;
        }
};

class SerialDevice {
    private:
        const char *name;
        SerialQueue q_host; // host -> dev queue
        SerialQueue q_dev;  // dev -> host queue

    public:
        SerialDevice(const char *name);
        ~SerialDevice();

        /*
           This method only exits when it receives a SHUTDOWN message. Otherwise
           processes in a loop forever.
           Must ONLY q_host->get() and q_dev->send() to prevent race conditions.
        */

        virtual void device_loop() = 0;
};
