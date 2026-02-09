#pragma once
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <iostream>
#include "util/printf_helper.hpp"

#define NUM_EVENTS 2'000'000
#define LAST_SAMPLE 999999999999999999
#define EVENT_BUFFER_SIZE 128000

struct event_wdata_t {
    uint64_t cycle;
    uint64_t data;
};

template <typename Data_T>
class EventBufferBase {
    public:
        virtual bool add_event(Data_T cycle) = 0;
        virtual bool peek_oldest(Data_T& cycle) = 0;
        virtual Data_T peek() = 0;
        virtual void pop() = 0;
        virtual bool pop_oldest(Data_T& cycle) = 0;
        virtual void dump_event_data(void) = 0;
        virtual ~EventBufferBase() = default;

        uint64_t first_event = 0;
        uint64_t last_event = 0;

        virtual bool write_event_data(const char *filename) = 0;

        virtual bool load_event_data(const char *filename) {
            // load 'recording' file into the log
            FILE *recording = fopen(filename, "r");
            if (!recording) {
                std::cerr << "Error: Could not open recording file\n";
                return(false);
            }
            
            Data_T event = {0, 0};
            int i = 0;

            while (fscanf(recording, "%llu", &event.cycle) != EOF) {
                if (!add_event(event)) {
                    printf("Event buffer full\n");
                    break;
                }
                if (first_event == 0) {
                    first_event = event.cycle;
                }
                last_event = event.cycle;
            }
            add_event({LAST_SAMPLE, 0});
            fclose(recording);
            return(true);
        }

        void synthesize_event_data (int frequency, double input_rate, int seconds) {
            double cycles_per_wave = (input_rate) / (frequency * 2);
            double cycle_at = 0.0f;
            int i = 0;
            Data_T event = {0, 0};
            while (i < ((seconds * input_rate) / (cycles_per_wave))) {
                event.cycle = cycle_at;
                add_event(event);
                cycle_at += cycles_per_wave;
                i++;
            }
            last_event = (uint64_t)(cycle_at);
            event.cycle = LAST_SAMPLE;
            add_event(event);
        }

        void print_event_metadata(void) {
            printf("first_event: %llu, last_event: %llu\n", u64_t(first_event), u64_t(last_event));
        }
    };
    
class EventBuffer : public EventBufferBase<event_wdata_t> {
    public:
    
        event_wdata_t *events;
        int read_pos = 0;
        int write_pos = 0;
    
        EventBuffer(uint64_t size) {
            events = new event_wdata_t[size];
            memset(events, 0, size * sizeof(event_wdata_t));
            read_pos = 0;
            write_pos = 0;
        }
        ~EventBuffer() {
            delete[] events;
        };

        bool write_event_data(const char *filename) override {
            FILE *file = fopen(filename, "wb");
            if (!file) {
                return false;
            }
            for (uint64_t i = 0; i < write_pos; i++) {
                fprintf(file, "%llu %llu\n", u64_t(events[i].cycle), u64_t(events[i].data));
            }
            fclose(file);
            return true;
        }

        inline bool add_event(event_wdata_t event) override {
            events[write_pos++] = event;
            return true;
        }
        inline event_wdata_t peek() override {
            return events[read_pos];
        }
        inline void pop() override {
            read_pos++;
        }
        inline bool peek_oldest(event_wdata_t& event) override {
            if (read_pos >= write_pos) {
                return false;
            }
            event = events[read_pos];
            return true;
        };
        inline bool pop_oldest(event_wdata_t& event) override {
            if (read_pos >= write_pos) {
                return false;
            }
            event = events[read_pos];
            read_pos++;
            return true;
        }
        void dump_event_data(void) override {
            for (uint64_t i = 0; i < write_pos; i++) {
                printf("%llu ", u64_t(events[i].cycle));
                if (events[i].cycle == LAST_SAMPLE) {
                    break;
                }
            }
            printf("\n");
        }

    };
    
    class EventBufferRing : public EventBufferBase<event_wdata_t> {
    public:
        event_wdata_t *events;
        int write_pos;
        int read_pos;
        int count;
        uint64_t size;
    
        EventBufferRing(uint64_t size) {
            this->size = size;
            events = new event_wdata_t[size];
            memset(events, 0, size * sizeof(event_wdata_t));
            write_pos = 0;
            read_pos = 0;
            count = 0;
        }
        ~EventBufferRing() {
            delete[] events;
        };

        bool write_event_data(const char *filename) override {
            FILE *file = fopen(filename, "wb");
            if (!file) {
                return false;
            }
            uint64_t i = read_pos;
            while (i != write_pos) {
                fprintf(file, "%llu %llu\n", u64_t(events[i].cycle), u64_t(events[i].data));
                i = (i + 1) % size;
            }
            fclose(file);
            return true;
        }

        bool add_event(event_wdata_t event) override {
            if (count >= size) {
                return false; // Buffer full
            }
            
            events[write_pos] = event;
            write_pos = (write_pos + 1) % size;
            count++;
            return true;
        }
        inline event_wdata_t peek() override {
            if (read_pos == write_pos) {
                return {LAST_SAMPLE, 0};
            }
            return events[read_pos];
        }
        inline void pop() override {
            read_pos = (read_pos + 1) % size;
            count--;
        }
        inline bool peek_oldest(event_wdata_t& event) override {
            if (count == 0) {
                event = {LAST_SAMPLE, 0};
                return false; // Buffer empty
            }
            event = events[read_pos];
            return true;
        }
    
        inline bool pop_oldest(event_wdata_t& event) override {
            if (count == 0) {
                event = {LAST_SAMPLE, 0};
                return false; // Buffer empty
            }
            
            event = events[read_pos];
            read_pos = (read_pos + 1) % size;
            count--;
            return true;
        }
        void dump_event_data(void) override {
        }
    };