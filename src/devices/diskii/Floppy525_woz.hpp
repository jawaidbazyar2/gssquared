#pragma once

#include "util/woz.hpp"
#include "FloppyDrive.hpp"

class Floppy525_woz: public FloppyDrive {
    bool enable = false;

    uint8_t rw_mode = 0;
    int8_t track = 0;
    uint8_t phase0 = 0;
    uint8_t phase1 = 0;
    uint8_t phase2 = 0;
    uint8_t phase3 = 0;
    uint8_t last_phase_on = 0;
    uint8_t Q7 = 0;
    uint8_t Q6 = 0;
    uint8_t write_protect = 0; // 1 = write protect, 0 = not write protect
    uint16_t image_index = 0;
    uint8_t write_shift_register = 0;

    // Woz image (holds the in-memory bitstream after load or import)
    Woz woz;
    // Cached pointer to current quarter-track's bitstream; nullptr = empty track.
    const woz_track_t *cur_track_ptr = nullptr;

    // Physical disk position in 1/8-bit-cell units.
    //   Bit index into track = bit_fp >> 3  (mod track_bits)
    //   1 bit cell = 4 CPU cycles → 1 CPU cycle = 2 units.
    // Advances by elapsed_cycles × 2 each fast_forward() call and is kept
    // wrapped within one revolution (track_bits × 8 units).
    uint64_t bit_fp = 0;
    uint64_t last_cycle = 0;

    // Apple II LSS shift accumulator (internal; cleared on each latch event).
    uint8_t lss_shift = 0;
    // Data latch: set to lss_shift whenever lss_shift's bit 7 becomes 1.
    // This is what the CPU reads via Q6L.  Held until the next latch event.
    uint8_t read_shift_register = 0;

    bool is_mounted = false;
    bool modified = false;
    media_descriptor *media_d = nullptr;

    uint64_t mark_cycles_turnoff = 0;

    // Advance bit_fp by elapsed cycles and shift the corresponding bits through
    // read_shift_register.  Tracks are circular; wrapping never resets the register.
    void fast_forward();

    // Refresh cur_track_ptr from woz.get_track(track).
    void update_track_ptr();

    public:
    Floppy525_woz(SoundEffect *sound_effect, NClockII *clock) : FloppyDrive(sound_effect, clock) {}

    virtual bool get_Q7() { return Q7; }
    virtual bool get_Q6() { return Q6; }

    virtual void set_track(int track_num) override;
    virtual int get_track() override { return track; }
    virtual void move_head(int direction) override;

    virtual void write_nybble(uint8_t nybble) override;
    virtual uint8_t read_nybble() override;

    virtual bool mount(uint64_t key, media_descriptor *media) override;
    virtual bool unmount(uint64_t key) override;
    virtual bool writeback() override;
    virtual drive_status_t status() override;
    virtual void reset() override;

    virtual bool get_enable() override { return enable; }
    virtual void set_enable(bool enable) override { this->enable = enable; }

    virtual uint8_t read_cmd(uint16_t address) override;
    virtual void write_cmd(uint16_t address, uint8_t data) override;

    void debug(DebugFormatter *f) {
        f->addLine("Track: %d", track);
        f->addLine("Bit Position: %llu", (unsigned long long)(bit_fp >> 3));
        f->addLine("LSS Shift: %02X", lss_shift);
        f->addLine("Data Latch: %02X", read_shift_register);
        f->addLine("Write Shift Register: %02X", write_shift_register);
        f->addLine("Q6: %d", Q6);
        f->addLine("Q7: %d", Q7);
        f->addLine("Modified: %d", modified);
    }
};
