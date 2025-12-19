#include <vector>
#include <SDL3/SDL.h>
#include <cassert>

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
    virtual void reset(uint8_t cmd, uint8_t reg);
    virtual void flush(uint8_t cmd, uint8_t reg);
    virtual void listen(uint8_t command, uint8_t reg);
    virtual ADB_Register talk(uint8_t command, uint8_t reg);
    virtual bool process_event(SDL_Event &event);
};

class ADB_Keyboard : public ADB_Device
{
    ADB_Keyboard(uint8_t id = 0x02) : ADB_Device(id) { }

    bool process_event(SDL_Event &event) {
        return false;
    }
};

class ADB_Mouse : public ADB_Device
{
    ADB_Mouse(uint8_t id = 0x03) : ADB_Device(id) { }

    bool process_event(SDL_Event &event) {
        return false;
    }
};


class ADB_Host
{
    std::vector<ADB_Device> devices;

    ADB_Host() { }

    void add_device(uint8_t id, ADB_Device device)
    {
        devices.push_back(device);
    }

    bool reset(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            device.reset(cmd, reg);
        }
        return true;
    }

    bool flush(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            device.flush(cmd, reg);
        }
        return true;
    }

    bool listen(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            if (device.get_id() == addr) {
                device.listen(cmd, reg);
                return true;
            }
        }
        return false;
    }

    bool talk(uint8_t addr, uint8_t cmd, uint8_t reg) {
        for (auto &device : devices) {
            if (device.get_id() == addr) {
                device.talk(cmd, reg);
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
            case 0b10: return listen(addr, cmd, reg); break;
            case 0b11: return talk(addr, cmd, reg); break;
            default:
                assert(false);
        }
    }

    bool process_event(SDL_Event &event) {
        for (auto &device : devices) {
            if (device.process_event(event)) {
                return true;
            }
        }
        return false;
    }

};
