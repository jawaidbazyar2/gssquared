#pragma once
#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <iostream>

#define NUM_EVENTS 2'000'000
#define LAST_SAMPLE 999999999999999999
#define EVENT_BUFFER_SIZE 128000

class EventBufferBase {
    public:
        virtual bool add_event(uint64_t cycle) = 0;
        virtual bool peek_oldest(uint64_t& cycle) = 0;
        virtual uint64_t peek() = 0;
        virtual void pop() = 0;
        virtual bool pop_oldest(uint64_t& cycle) = 0;
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
            uint64_t event;
            int i = 0;

            while (fscanf(recording, "%llu", &event) != EOF) {
                if (!add_event(event)) {
                    printf("Event buffer full\n");
                    break;
                }
                if (first_event == 0) {
                    first_event = event;
                }
                last_event = event;
            }
            add_event(LAST_SAMPLE);
            fclose(recording);
            return(true);
        }

        void synthesize_event_data (int frequency, double input_rate, int seconds) {
            double cycles_per_wave = (input_rate) / (frequency * 2);
            double cycle_at = 0.0f;
            int i = 0;
            while (i < ((seconds * input_rate) / (cycles_per_wave))) {
                add_event(cycle_at);
                cycle_at += cycles_per_wave;
                i++;
            }
            last_event = (uint64_t)(cycle_at);
            add_event(LAST_SAMPLE);
        }

        void print_event_metadata(void) {
            printf("first_event: %llu, last_event: %llu\n", first_event, last_event);
        }
    };
    
class EventBuffer : public EventBufferBase {
    public:
    
        uint64_t *events;
        int read_pos = 0;
        int write_pos = 0;
    
        EventBuffer(uint64_t size) {
            events = new uint64_t[size];
            memset(events, 0, size * sizeof(uint64_t));
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
                fprintf(file, "%llu\n", events[i]);
            }
            fclose(file);
            return true;
        }

        inline bool add_event(uint64_t cycle) override {
            events[write_pos++] = cycle;
            return true;
        }
        inline uint64_t peek() override {
            return events[read_pos];
        }
        inline void pop() override {
            read_pos++;
        }
        inline bool peek_oldest(uint64_t& cycle) override {
            if (read_pos >= write_pos) {
                return false;
            }
            cycle = events[read_pos];
            return true;
        };
        inline bool pop_oldest(uint64_t& cycle) override {
            if (read_pos >= write_pos) {
                return false;
            }
            cycle = events[read_pos];
            read_pos++;
            return true;
        }
        void dump_event_data(void) override {
            for (uint64_t i = 0; i < write_pos; i++) {
                printf("%llu ", events[i]);
                if (events[i] == LAST_SAMPLE) {
                    break;
                }
            }
            printf("\n");
        }

    };
    
    class EventBufferRing : public EventBufferBase {
    public:
        uint64_t *events;
        int write_pos;
        int read_pos;
        int count;
        uint64_t size;
    
        EventBufferRing(uint64_t size) {
            this->size = size;
            events = new uint64_t[size];
            memset(events, 0, size * sizeof(uint64_t));
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
                fprintf(file, "%llu\n", events[i]);
                i = (i + 1) % size;
            }
            fclose(file);
            return true;
        }

        bool add_event(uint64_t cycle) override {
            if (count >= size) {
                return false; // Buffer full
            }
            
            events[write_pos] = cycle;
            write_pos = (write_pos + 1) % size;
            count++;
            return true;
        }
        inline uint64_t peek() override {
            if (read_pos == write_pos) {
                return LAST_SAMPLE;
            }
            return events[read_pos];
        }
        inline void pop() override {
            read_pos = (read_pos + 1) % size;
            count--;
        }
        inline bool peek_oldest(uint64_t& cycle) override {
            if (count == 0) {
                cycle = LAST_SAMPLE;
                return false; // Buffer empty
            }
            cycle = events[read_pos];
            return true;
        }
    
        inline bool pop_oldest(uint64_t& cycle) override {
            if (count == 0) {
                cycle = LAST_SAMPLE;
                return false; // Buffer empty
            }
            
            cycle = events[read_pos];
            read_pos = (read_pos + 1) % size;
            count--;
            return true;
        }
        void dump_event_data(void) override {
        }
    };