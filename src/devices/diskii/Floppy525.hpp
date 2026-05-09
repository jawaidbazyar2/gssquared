#pragma once

#include "devices/diskii/diskii_fmt.hpp"
#include "FloppyDrive.hpp"

class Floppy525: public FloppyDrive {
    bool enable = false;

    uint8_t rw_mode; // 0 = read, 1 = write
    int8_t track;
    uint8_t phase0;
    uint8_t phase1;
    uint8_t phase2;
    uint8_t phase3;
    uint8_t last_phase_on;
    uint8_t Q7 = 0;
    uint8_t Q6 = 0;
    uint8_t write_protect = 0; // 1 = write protect, 0 = not write protect
    uint16_t image_index = 0;
    uint16_t head_position = 0; // index into the track
    uint8_t bit_position = 0; // how many bits left in byte.
    uint16_t read_shift_register = 0; // when bit position = 0, this is 0. As bit_position increments, we shift in the next bit of the byte at head_position.
    uint8_t write_shift_register = 0; 
    uint64_t last_read_cycle = 0;

    bool is_mounted = false;
    bool modified = false;
    disk_image_t media;
    nibblized_disk_t nibblized;
    media_descriptor *media_d;

    uint64_t mark_cycles_turnoff = 0;

    public:
    Floppy525(SoundEffect *sound_effect, NClockII *clock) : FloppyDrive(sound_effect, clock) {
        enable = false;
        
        rw_mode = 0;
        track = 0;
        phase0 = 0;
        phase1 = 0;
        phase2 = 0;
        phase3 = 0;
        last_phase_on = 0;
        Q7 = 0;
        Q6 = 0;
        write_protect = 0;
        image_index = 0;
        head_position = 0;
        bit_position = 0;
        read_shift_register = 0;
        write_shift_register = 0;
    //    last_read_cycle = 0;
        is_mounted = false;
        modified = false;
        media_d = nullptr;
    }
    virtual bool get_Q7() { return Q7; }
    virtual bool get_Q6() { return Q6; }
    // The controller only lets one drive be on at a time, and that must be managed by the controller.
    //virtual void motor(bool onoff) override ;
/*     virtual void set_track(int track_num) override; */
    virtual int get_track() override { return track; } ;
    /* virtual void move_head(int direction) override; */

    virtual void write_nybble(uint8_t nybble) override;
    virtual uint8_t read_nybble() override;
    // these are unimplemented, in this old version of code.
    virtual void write_pulse(uint8_t bit) override {};
    virtual uint8_t read_pulse() override { return 0;};

    virtual bool mount(uint64_t key, media_descriptor *media) override;
    virtual bool unmount(uint64_t key) override;
    virtual bool writeback() override;
    //virtual void nibblize() override;
    virtual drive_status_t status() override;
    virtual void reset() override;

    virtual bool get_enable() override { return enable; }
    virtual void set_enable(bool enable) override { this->enable = enable; }
    virtual void set_phase(uint8_t phase, uint8_t onoff) override {}; // TODO: this is a dummy, not used by older 5.25
    //virtual void get_rdpulse() override {}; // TODO: this is a dummy, not used by older 5.25

    virtual uint8_t read_cmd(uint16_t address) override;
    virtual void write_cmd(uint16_t address, uint8_t data) override;

    void debug(DebugFormatter *f) {
        f->addLine("Track: %d", track);
        f->addLine("Head Position: %d", head_position);
        f->addLine("Bit Position: %d", bit_position);
        f->addLine("Read Shift Register: %02X", read_shift_register);
        f->addLine("Write Shift Register: %02X", write_shift_register);
        f->addLine("Q6: %d", Q6);
        f->addLine("Q7: %d", Q7);
        f->addLine("Modified: %d", modified);
    }
};