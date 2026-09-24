/*
 * 6522 Chip Emulation Test Harness
 */

#include "devices/mockingboard/W6522.hpp"
#include "util/InterruptController.hpp"
#include "devices/mockingboard/tests.hpp"

uint64_t debug_level = DEBUG_MOCKINGBOARD;


class NClockIIMB : public NClock {
    protected:
        bool slow_mode = false;
        
        // have a predefined vector of max 8 cycle handlers.
        std::vector<std::function<void()>> cycle_handlers;
    
    public:
        NClockIIMB(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) : NClock(clock_set, clock_mode) {
            clock_mode = CLOCK_1_024MHZ;
            // preallocate max size of 8
            cycle_handlers.reserve(8);
        }

        void set_cycle_handler(std::function<void()> cycle_handler) {
            cycle_handlers.push_back(cycle_handler);
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
                    
                    for (auto &cycle_handler : cycle_handlers) {
                        cycle_handler();
                    }
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
    N6522 chip("6522 #1 $80", &clock, &irq_controller);
    int test_num = 1;

    std::function<void()> cycle_handler = std::bind(&N6522::incr_cycle, &chip);
    clock.set_cycle_handler(cycle_handler);

    int recindex = 0;
    int mismatches = 0;

    reg_record_t *test_recs = recs[0];

    // take CLI argument to select test
    if (argc > 1) {
        test_num = atoi(argv[1]);
        // it's 1 .. N, not 0 .. N-1
        if (test_num >= 1 && test_num <= sizeof(recs) / sizeof(recs[0])) {
            test_recs = recs[test_num-1];
        }
    }
    printf("Running test %d\n", test_num);

    while (1) {
        uint64_t system_cycles = clock.get_vid_cycles();

        if (system_cycles >= test_recs[recindex].cycle) {
            if (test_recs[recindex].action == END) {
                break;
            }
            if (test_recs[recindex].action == WRITE) {
                chip.write(test_recs[recindex].reg, test_recs[recindex].value);
                //printf("write: Reg %02X <= %02X\n", recs[recindex].reg, recs[recindex].value);
            } else if (test_recs[recindex].action == READ) {
                uint8_t value = chip.read(test_recs[recindex].reg);
                //printf("read: Reg %02X => %02X\n", recs[recindex].reg, value);
            } else if (test_recs[recindex].action == READ_EXPECT) {
                uint8_t value = chip.read(test_recs[recindex].reg);
                if (value != test_recs[recindex].value) {
                    mismatches++;
                    printf("MISMATCH cycle=%llu reg=%02X expected=%02X actual=%02X\n",
                           system_cycles,
                           test_recs[recindex].reg,
                           test_recs[recindex].value,
                           value);
                } else {
                    printf("MATCH    cycle=%llu reg=%02X value=%02X\n",
                           system_cycles,
                           test_recs[recindex].reg,
                           value);
                }
            }
            recindex++;
        }

        printf("%8llu: ",system_cycles); chip.debug_one();
        clock.incr_cycles(); // this should trigger the cycle_handler..
        //chip.incr_cycle();
        printf("after   : "); chip.debug_one();
        printf("\n");
    }
    if (mismatches) {
        printf("FAIL: %d read expectation mismatch(es)\n", mismatches);
        return 1;
    }
    printf("PASS: all read expectations matched\n");
    return 0;
}