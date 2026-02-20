#pragma once

#include <cstdint>
#include "util/SoundEffect.hpp"
#include "util/media.hpp"
#include "NClock.hpp"
#include "util/mount.hpp"

class FloppyDrive {
protected:
    int track_num;
    int side;

    SoundEffect *sound_effect;
    NClockII *clock;

    public:
    FloppyDrive(SoundEffect *sound_effect, NClockII *clock) : sound_effect(sound_effect), clock(clock) {}

    //virtual void motor(bool) = 0;
    virtual void set_enable(bool enable) = 0;
    virtual bool get_enable() = 0;
    virtual void set_track(int track_num) = 0;
    virtual int get_track() = 0;
    virtual void write_nybble(uint8_t nybble) = 0;
    virtual uint8_t read_nybble() = 0;
    virtual void move_head(int direction) = 0;

    virtual bool mount(uint64_t key, media_descriptor *media) = 0;
    virtual bool unmount(uint64_t key) = 0;
    virtual bool writeback() = 0;
    virtual void nibblize() = 0;
    virtual drive_status_t status() = 0;
    virtual void reset() = 0;

    virtual void play_sound(uint64_t sound_effect_id) {
        sound_effect->play(sound_effect_id);
    }

    virtual uint8_t read_cmd(uint16_t address) = 0;
    virtual void write_cmd(uint16_t address, uint8_t data) = 0;

};

