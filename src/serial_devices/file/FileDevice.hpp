#pragma once

#include <SDL3/SDL.h>
#include <cstdio>

#include "serial_devices/SerialDevice.hpp"
#include "util/printf_helper.hpp"

class FileDevice : public SerialDevice {
    private:
        FILE *file;
        uint32_t frames_since_last_write = 0;

    public:
        FileDevice(const char *name, const char *port_id) : SerialDevice("FileDevice", port_id) {
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
            close_file();
        }

        void open_file() {
            // construct file name from "GS2.{Extension}.{YYYYMMDDHHMMSS}"
            time_t now = time(NULL);
            struct tm *tm_now = localtime(&now);
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", tm_now);

            char filename_buf[256];
            snprintf(filename_buf, 256, "GS2.%s.%s", port_id, timestamp);
            printf("FileDevice: opening file %s\n", filename_buf);

            file = fopen(filename_buf, "w");
            if (file == NULL) {
                printf("Failed to open file %s\n", filename_buf);
                return;
            }
        }

        void close_file() {
            if (file != NULL) {
                fclose(file);
                file = NULL;
            }
        }

        void device_loop() override {
            while (true) {
                SDL_Delay(10);
                uint64_t bytes_received = 0;
                uint64_t bytes_in_q = q_host.get_count();
                frames_since_last_write++;
                
                if (file && frames_since_last_write > 1'000) {  // 10 seconds, 10 ms = 1000 frames.
                    frames_since_last_write = 0;
                    close_file();
                }

                if (bytes_in_q > 0) {
                    printf("FileDevice: %llu bytes in q\n", u64_t(bytes_in_q));
                    while (!q_host.is_empty()) {
                        SerialMessage msg = q_host.get();
                        switch (msg.type) {
                            case MESSAGE_SHUTDOWN:
                                // we're done, just return    
                                printf("FileDevice: shutting down\n");
                                return;
                            case MESSAGE_DATA:
                                if (!file) open_file();
                                fwrite(&msg.data, 1, 1, file);
                                //if (msg.data == 0x0D) q_dev.send(msg); // try echoing the CR back to the host.
                                break;
                            default:
                                break;
                        }
                    }
                    printf("EchoDevice: %llu bytes received\n", u64_t(bytes_received));
                }
            }
        }
};
