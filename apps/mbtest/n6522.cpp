/*
 * 6522 Chip Emulation Test Harness
 */

#include "devices/mockingboard/N6522.hpp"

uint64_t debug_level = DEBUG_MOCKINGBOARD;


enum rw {
    READ =0,
    WRITE = 1,
    END = 2,
};

struct reg_record_t {
    rw action;
    uint64_t cycle;
    uint8_t reg;
    uint8_t value;
};

reg_record_t recs1[] = {
    // we're in 1-shot mode.
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {WRITE, 20, MB_6522_T1C_H, 0x00}, // write T1C-H, should clear interrupt pending and set up for another 1-shot

    {WRITE, 29, MB_6522_ACR, 0x40},
    {WRITE, 30, MB_6522_T1C_H, 0x00},
    {WRITE, 40, MB_6522_IFR, 0x40}, // clear T1 interrupt
    {WRITE, 60, MB_6522_IER,0b10100000}, // enable T2 interrupt also
    {WRITE, 61, MB_6522_T2L_L, 0x05},
    {WRITE, 62, MB_6522_T2C_H, 0x00},

    {WRITE, 79, MB_6522_IER, 0x40}, // disable T1 interrupt
    {WRITE, 80, MB_6522_T1L_L, 10},
    {WRITE, 81, MB_6522_T1C_H, 0x00},
    {WRITE, 85, MB_6522_T1L_L, 10},
    {WRITE, 86, MB_6522_T1C_H, 0x00},
    {WRITE, 90, MB_6522_T1L_L, 10},
    {WRITE, 91, MB_6522_T1C_H, 0x00},
    

    // should have a concept here to READ the register.
    {READ, 100, MB_6522_T2L_L, 0x00},
    {END, 0, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 0, should cycle 1.5 times before IRQ.
reg_record_t recs2[] = {
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x00},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {END, 0, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 5
reg_record_t recs3[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {END, 0, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to FFFF
reg_record_t recs4[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0xFF},
    {WRITE, 8, MB_6522_T1C_H, 0xFF},
    {END, 65550, 0, 0},
};

// Test:
// Continuous mode, T1 Latch set to 5, then go back to one-shot mode.
reg_record_t recs5[] = {    
    {WRITE, 1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {WRITE, 2, MB_6522_ACR, 0x40},
    {WRITE,5, MB_6522_T1L_L, 0x05},
    {WRITE, 8, MB_6522_T1C_H, 0x00},
    {WRITE, 18, MB_6522_ACR, 0x00}, // turn off continuous mode
    {END, 0, 0, 0},
};

reg_record_t *recs[] = {
    recs1,
    recs2,
    recs3,
    recs4,
    recs5,
};

class NClockIIMB : public NClock {
    protected:
        bool slow_mode = false;
    
    public:
        NClockIIMB(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) : NClock(clock_set, clock_mode) {
            clock_mode = CLOCK_1_024MHZ;
        }
    
        // II, II+, IIe
        inline virtual void slow_incr_cycles() override {
            add_cpu();
            add_c14m(current.c_14M_per_cpu_cycle);

                video_cycle_14M_count += current.c_14M_per_cpu_cycle;
                scanline_14M_count += current.c_14M_per_cpu_cycle;
    
                if (video_cycle_14M_count >= 14) {
                    video_cycle_14M_count -= 14;
                    //video_scanner->video_cycle();
                    tick_vid();
                }
                if (scanline_14M_count >= 910) {  // end of scanline
                    add_c14m(current.extra_per_scanline);
                    scanline_14M_count = 0;
                }

        }
    
        inline void set_slow_mode(bool value) { slow_mode = value; }
        inline bool get_slow_mode() { return slow_mode; }
    
    
        virtual DebugFormatter *debug() override {
            DebugFormatter *f = NClock::debug();
            f->addLine("CPU Slow Mode: %d", get_slow_mode());
    
            return f;
        }
    
    };

int main(int argc, char *argv[]) {
    InterruptController irq_controller;
    NClockIIMB clock(CLOCK_SET_US, CLOCK_1_024MHZ);

    N6522 chip("6522 #1 $80", &clock, &irq_controller, &clock.vid);

    int recindex = 0;
    reg_record_t *test_recs = recs[0];

    // take CLI argument to select test
    if (argc > 1) {
        int test_num = atoi(argv[1]);
        // it's 1 .. N, not 0 .. N-1
        if (test_num >= 1 && test_num <= sizeof(recs) / sizeof(recs[0])) {
            test_recs = recs[test_num-1];
        }
    }

    /*
    cycle 0: everything reset to 0
    cycle N: set counter
    cycle N+1: the new counter value can be read here.
    
    current cycle: 0
    call read/write routine
    increment cycle
    new cycle: 1
    */

    while (1) {
        uint64_t system_cycles = clock.get_vid_cycles();
        // this section simulates the CPU doing things, changing the registers.

        // this section triggers our per-instruction timer callbacks.
        /*
        Note: in test harness, we are incrementing one clock at a time. In real emulator,
        we will add 2 to 7/8 cycles at a time. The potential issue here is the difference
        between CPU having noticed the interrupt in the penultimate cycle, which this code will
        NOT DO. This may be a distinction w/o a difference in real code, but, bear in mind that
        we may be required to use the single-cycle version.
        */
        clock.vid.process_due();
       
        // here, basically reads/writes registers in same position our CPU will
        printf("%8llu: ",system_cycles); chip.debug_one();

        if (test_recs[recindex].action == END) {
            if (test_recs[recindex].cycle == 0 || system_cycles >= test_recs[recindex].cycle) {
                break;
            }
        } else if (system_cycles >= test_recs[recindex].cycle) {
                if (test_recs[recindex].action == WRITE) {
                    chip.write(test_recs[recindex].reg, test_recs[recindex].value);
                    //printf("write: Reg %02X <= %02X\n", recs[recindex].reg, recs[recindex].value);
                } else {
                    uint8_t value = chip.read(test_recs[recindex].reg);
                    //printf("read: Reg %02X => %02X\n", recs[recindex].reg, value);
                }
                recindex++;
        }

        clock.incr_cycles();

        printf("after   : ");chip.debug_one();
       /*  DebugFormatter *df = clock.debug();
        df->print();
        delete df; */
        printf("\n");

    }
    return 0;
}