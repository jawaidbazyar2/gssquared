#pragma once

#include "ADB_Device.hpp"

class ADB_Mouse : public ADB_Device
{
    public:
    ADB_Mouse(uint8_t id = 0x03) : ADB_Device(id) { }

    void reset(uint8_t cmd, uint8_t reg) override { }
    void flush(uint8_t cmd, uint8_t reg) override { }
    void listen(uint8_t command, uint8_t reg) override { }
    ADB_Register talk(uint8_t command, uint8_t reg) override {
        ADB_Register reg_result = {};
        return reg_result;
    }
    bool process_event(SDL_Event &event) override {
        return false;
    }
};
