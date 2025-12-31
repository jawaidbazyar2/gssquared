#pragma once

#include "ADB_Device.hpp"


struct adb_mod_key_t {
    union {
        uint8_t value;
        struct {
            uint8_t shift: 1;
            uint8_t ctrl: 1;
            uint8_t caps: 1;
            uint8_t repeat: 1;
            uint8_t keypad: 1;
            uint8_t updated: 1;
            uint8_t closed: 1;
            uint8_t open: 1;
        };
    };
};

class ADB_Keyboard : public ADB_Device
{
    public:
    ADB_Keyboard(uint8_t id = 0x02) : ADB_Device(id) { }

    void reset(uint8_t cmd, uint8_t reg) override { }
    void flush(uint8_t cmd, uint8_t reg) override { }
    void listen(uint8_t command, uint8_t reg) override { }
    ADB_Register talk(uint8_t command, uint8_t reg) override {
        ADB_Register reg_result = {};
        return reg_result;
    }
    bool process_event(SDL_Event &event) override {
        // register 0 is keyboard data.
        uint8_t k1_released = 0;
        uint8_t k2_released = 0;
        if (event.type == SDL_EVENT_KEY_DOWN) {
            printf("Key down: %08X\n", event.key.key);
            k1_released = 0;
            
        }
        if (event.type == SDL_EVENT_KEY_UP) {
            printf("Key up: %08X\n", event.key.key);
            k1_released = 1;
        }
        return false;
    }
};

