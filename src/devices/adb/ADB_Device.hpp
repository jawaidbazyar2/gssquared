#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

class ADB_Register 
{
    uint32_t size;
    uint8_t data[8];
};


class ADB_Device
{
    uint8_t id;

    ADB_Register registers[4];
public:
    ADB_Device(uint8_t id) : id(id) { }
    uint8_t get_id() { return id; }
    virtual void reset(uint8_t cmd, uint8_t reg) = 0;
    virtual void flush(uint8_t cmd, uint8_t reg) = 0;
    virtual void listen(uint8_t command, uint8_t reg) = 0;
    virtual ADB_Register talk(uint8_t command, uint8_t reg) = 0;
    virtual bool process_event(SDL_Event &event) = 0;
};

