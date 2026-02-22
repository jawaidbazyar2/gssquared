#pragma once

#include <cstdint>
#include "util/SoundEffect.hpp"
#include "NClock.hpp"
#include "util/media.hpp"
#include "util/StorageDevice.hpp"

/* This file is little-endian dependent */

static const uint32_t IWM_SWITCH_COUNT = 8;
static const uint32_t IWM_ADDRESS_MAX = IWM_SWITCH_COUNT * 2;

enum iwm_switch_t {
    IWM_CA0_OFF = 0,
    IWM_CA0_ON = 1,
    IWM_CA1_OFF = 2,
    IWM_CA1_ON = 3,
    IWM_CA2_OFF = 4,
    IWM_CA2_ON = 5,
    IWM_LSTRB_OFF = 6,
    IWM_LSTRB_ON = 7,
    IWM_ENABLE_OFF = 8,
    IWM_ENABLE_ON = 9,
    IWM_SELECT_OFF = 10,
    IWM_SELECT_ON = 11,
    IWM_Q6_OFF = 12,
    IWM_Q6_ON = 13,
    IWM_Q7_OFF = 14,
    IWM_Q7_ON = 15,
};

class IWM_Drive : public StorageDevice {
    protected:
    bool enabled = false;
    bool motor_on = false;
    bool sense_input = false;
    bool led_status = false;

    SoundEffect *sound_effect;

    public:
    IWM_Drive(SoundEffect *sound_effect, NClockII *clock) {
        sound_effect = sound_effect;
        clock = clock;
        enabled = false;
        motor_on = false;
        sense_input = false;
        led_status = false;
    }
    virtual ~IWM_Drive() {};
    virtual int get_track() = 0;
    virtual void set_enable(bool enable) {};
    bool get_enabled() { return enabled; }
    bool get_motor_on() { return motor_on; }
    bool get_sense_input() { return sense_input; }
    bool get_led_status() { return led_status; }
    virtual void read_cmd(uint16_t address) {};
    virtual void write_cmd(uint16_t address, uint8_t data) {};
    virtual void write_data_register(uint8_t data) {};
    virtual uint8_t read_data_register() { return 0; }
    
    virtual bool mount(uint64_t key, media_descriptor *media);
    virtual bool unmount(uint64_t key);
    virtual bool writeback(uint64_t key);
    virtual drive_status_t status(uint64_t key);
};

