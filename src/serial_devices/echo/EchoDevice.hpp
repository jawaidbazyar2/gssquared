#pragma once

#include <SDL3/SDL.h>
#include <cstdio>

#include "serial_devices/SerialDevice.hpp"
#include "util/printf_helper.hpp"

class EchoDevice : public SerialDevice {
    public:
        EchoDevice(const char *name, const char *port_id) : SerialDevice("EchoDevice", port_id) {
        }

        void device_loop() override {
            while (true) {
                SDL_Delay(17);
                uint64_t bytes_received = 0;
                uint64_t bytes_in_q = q_host.get_count();
                
                if (bytes_in_q > 0) {
                    printf("EchoDevice: %llu bytes in q\n", u64_t(bytes_in_q));
                    while (!q_host.is_empty()) {
                        SerialMessage msg = q_host.get();
                        switch (msg.type) {
                            case MESSAGE_SHUTDOWN:
                                // we're done, just return    
                                printf("EchoDevice: shutting down\n");
                                return;
                            case MESSAGE_DATA:
                                q_dev.send(msg);
                                bytes_received += 1;
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
