#include <vector>
#include <SDL3/SDL.h>
#include <cassert>

#include "ADB_Device.hpp"


class ADB_Host
{
    std::vector<ADB_Device *> devices;

    public:
    ADB_Host() { }

    void add_device(uint8_t id, ADB_Device *device)
    {
        devices.push_back(device);
    }

    bool reset(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            device->reset(cmd, reg);
        }
        return true;
    }

    bool flush(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            device->flush(cmd, reg);
        }
        return true;
    }

    bool listen(uint8_t addr, uint8_t cmd, uint8_t reg, ADB_Register &msg) {
        for (auto &device : devices) {
            if (device->get_id() == addr) {
                device->listen(cmd, reg, msg);
                return true;
            }
        }
        return false;
    }

    bool talk(uint8_t addr, uint8_t cmd, uint8_t reg, ADB_Register &msg) {
        for (auto &device : devices) {
            if (device->get_id() == addr) {
                msg = device->talk(cmd, reg);
                return true;
            }
        }
        return false;
    }

    bool send_command(uint8_t command, ADB_Register &msg) {
        uint8_t addr = (command & 0xF0) >> 4;
        uint8_t cmd = (command & 0b1100) >> 2;
        uint8_t reg = (command & 0b0011);

        switch (cmd) {
            case 0b00: return reset(addr, cmd, reg); break;
            case 0b01: return flush(addr, cmd, reg); break;
            case 0b10: return listen(addr, cmd, reg, msg); break;
            case 0b11: return talk(addr, cmd, reg, msg); break;
            default:
                assert(false);
        }
    }

    bool process_event(SDL_Event &event) {
        for (auto &device : devices) {
            if (device->process_event(event)) {
                return true;
            }
        }
        return false;
    }

    void debug_display(DebugFormatter *df) {
        for (auto &device : devices) {
            device->debug_display(df);
        }
    }

};

