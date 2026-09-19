#pragma once

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <SDL3/SDL.h>

enum serial_message_type_t {
    MESSAGE_NONE = 0,
    MESSAGE_DATA,
    MESSAGE_BREAK_ON,
    MESSAGE_BREAK_OFF,
    MESSAGE_CLOSE,
    MESSAGE_SHUTDOWN,
    MESSAGE_LINE,
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
        inline uint32_t space() {
            return (queue_depth - 1) - static_cast<uint32_t>(get_count());
        }
};

class SerialDevice {
    public:
        static constexpr uint8_t MODEM_CD  = 1 << 0;
        static constexpr uint8_t MODEM_CTS = 1 << 1;
        static constexpr uint8_t MODEM_DSR = 1 << 2;
        static constexpr uint8_t MODEM_INPUTS_ASSERTED = MODEM_CD | MODEM_CTS | MODEM_DSR;

    protected:
        const char *name;
        const char *port_id;
        SDL_Thread *thread;

        /* Host-logical handshake inputs: 1 = line asserted (carrier / ready).
         * Worker writes; emu thread reads. Chips invert where silicon requires it.
         * Not a queue message — pins are levels; IRQ edges are synthesized on the chip. */
        std::atomic<uint8_t> modem_inputs_{MODEM_INPUTS_ASSERTED};

    public:
        SerialQueue q_host; // host -> dev queue
        SerialQueue q_dev;  // dev -> host queue

        SerialDevice(const char *name, const char *port_id);
        virtual ~SerialDevice();

        /*
           This method only exits when it receives a SHUTDOWN message. Otherwise
           processes in a loop forever.
           Must ONLY q_host->get() and q_dev->send() to prevent race conditions.
           Handshake inputs are the exception: set_modem_inputs() from the worker.
        */

        const char *get_name() { return name; }
        const char *get_port_id() const { return port_id; }
        virtual void device_loop() = 0;

        void set_modem_inputs(uint8_t bits) {
            modem_inputs_.store(bits, std::memory_order_relaxed);
        }
        uint8_t modem_inputs() const {
            return modem_inputs_.load(std::memory_order_relaxed);
        }

        /** HUD: handshake plus backend status. Safe to call from the UI thread. */
        virtual void format_hud_status(char *buf, size_t n) const {
            format_handshake(buf, n);
        }

    protected:
        void format_handshake(char *buf, size_t n) const {
            const uint8_t m = modem_inputs();
            std::snprintf(buf, n, "CD%c CTS%c DSR%c",
                          (m & MODEM_CD) ? '+' : '-',
                          (m & MODEM_CTS) ? '+' : '-',
                          (m & MODEM_DSR) ? '+' : '-');
        }
};
