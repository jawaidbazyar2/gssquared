#pragma once

#include "util/woz.hpp"
#include "FloppyDrive.hpp"

class Floppy525_woz: public FloppyDrive {

    uint8_t rw_mode = 0;

    uint8_t phase0 = 0;
    uint8_t phase1 = 0;
    uint8_t phase2 = 0;
    uint8_t phase3 = 0;
    bool enable = false; // "enable" or drive off/on

    uint8_t write_protect = 0; // 1 = write protect, 0 = not write protect
    uint16_t image_index = 0;
    int16_t track = 0;

    // Woz image (holds the in-memory bitstream after load or import)
    Woz woz;

    // Cached pointer to current quarter-track's bitstream; nullptr = empty track.
    // Non-const because write_pulse() needs to mutate the bits in place.
    woz_track_t *cur_track_ptr = nullptr;

    // Physical disk position in 1/8-bit-cell units.
    //   Bit index into track = bit_fp >> 3  (mod track_bits)
    //   1 bit cell = 4 CPU cycles → 1 CPU cycle = 2 units.
    // Advances by elapsed_cycles × 2 each fast_forward() call and is kept
    // wrapped within one revolution (track_bits × 8 units).
    uint64_t last_cycle = 0;
    uint64_t read_position = 0; // how far the emulation has progressed consuming bits from the track
    uint64_t head_position = 0; // the simulated head position.

    bool is_mounted = false;
    bool modified = false;
    media_descriptor *media_d = nullptr;

    uint64_t random_bits = 0x5FCB9E767DC3523A;
    uint32_t windowBits = 0;
    
    uint64_t mark_cycles_turnoff = 0;

    FILE *dbglog = nullptr;
    EventTimer *event_timer = nullptr;

    // Refresh cur_track_ptr from woz.get_track(track).
    void update_track_ptr();
    void update_track();
    static void phase_change_callback(uint64_t instanceID, void *userData);

    void decode(uint8_t reg);
    uint8_t get_random_bit();

public:
    Floppy525_woz(SoundEffect *sound_effect, NClockII *clock, EventTimer *event_timer) : FloppyDrive(sound_effect, clock) {
        this->event_timer = event_timer;
    }
    ~Floppy525_woz() { };

    // TODO: these should all be virtual, both 5.25 and 3.5 implement them (but their behavior is radically different)
    virtual void set_phase(uint8_t phase, bool onoff) override;

    virtual void set_enable(bool enable) override {
        if (!this->enable && enable) {
            // we are turning the motor on
            last_cycle = clock->get_cycles();
        }
        this->enable = enable;

        // TODO: various state we probably need to clear here if they turn enable off. Also,
        // enable=1 means motor on, motor sound is handled by controller class.
    }

    // Advance bit_fp by elapsed cycles and shift the corresponding bits through
    // read_shift_register.  Tracks are circular; wrapping never resets the register.
    uint64_t fast_forward(uint64_t now);
    uint8_t read_pulse() override;
    void    write_pulse(uint8_t bit) override;
    inline uint8_t get_write_protect() { return write_protect; }

    virtual int get_track() override { return track; }

    virtual void write_nybble(uint8_t nybble) override {};
    virtual uint8_t read_nybble() override { return 0;};

    virtual bool mount(uint64_t key, media_descriptor *media) override;
    virtual bool unmount(uint64_t key) override;
    virtual bool writeback() override;
    virtual drive_status_t status() override;
    virtual void reset() override;

    /** TODO: DEPRECATED     */
    virtual bool get_enable() override { return enable; }

    virtual uint8_t read_cmd(uint16_t address) override;
    virtual void write_cmd(uint16_t address, uint8_t data) override;
    /** END DEPRECATED     */

    void set_dbglog(FILE *dbglog) { this->dbglog = dbglog; }

    void debug(DebugFormatter *f) {
        f->addLine("Image: %s", woz.get_current_filename().c_str());
        f->addLine("enable: %d ph [%d,%d,%d,%d]", enable, phase0, phase1, phase2, phase3);
        f->addLine("Track: %d.%d", track/4, track%4);
        f->addLine("Track Bits: %llu", (unsigned long long)(cur_track_ptr ? cur_track_ptr->bit_count : 0));

        uint64_t pos = head_position>>3;
        f->addLine("Head Position: %llu (%d.%d)", pos, pos / 8, pos % 8);
        pos = read_position>>3;
        f->addLine("Read Position: %llu (%d.%d)", pos, pos / 8, pos % 8);       
        f->addLine("Last Cycle: %llu", (unsigned long long)last_cycle);
        f->addLine("Modified: %d", modified);
    }
};
