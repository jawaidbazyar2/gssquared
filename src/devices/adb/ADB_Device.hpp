#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

struct ADB_Register 
{
    uint32_t size;
    uint8_t data[8];
};

#define ADB_SR_ENABLE 0x20

class ADB_Device
{
protected:
    uint8_t id;
    ADB_Register registers[4];

public:
    ADB_Device(uint8_t id) : id(id) { 
        registers[1].size = 0;
        registers[2].size = 0;

        registers[3].size = 2;
        registers[3].data[0] = 0;
        registers[3].data[1] = id | ADB_SR_ENABLE;
    }
    uint8_t get_id() { return id; }
    virtual void reset(uint8_t cmd, uint8_t reg) = 0;
    virtual void flush(uint8_t cmd, uint8_t reg) = 0;
    virtual void listen(uint8_t command, uint8_t reg, ADB_Register &msg) = 0;
    virtual ADB_Register talk(uint8_t command, uint8_t reg) = 0;
    virtual bool process_event(SDL_Event &event) = 0;

    void print_registers() {
        printf("%02d: Registers: ", id);
        for (int i = 0; i < 4; i++) {
            printf(" %02X: [", i);
            // print from MSB to LSB
            for (int j = registers[i].size - 1; j >= 0; j--) {
                printf("%02X ", registers[i].data[j]);
            }
            printf("]");
        }
        printf("\n");
    }
};

