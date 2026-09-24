/*
 * 6522 Chip Emulation Test Harness
 */

#include "devices/mockingboard/W6522.hpp"
#include "NClock.hpp"
#include "cpus/cpu_implementations.cpp"
#include "mmus/mmu.hpp"
#include "util/InterruptController.hpp"

uint64_t debug_level = 0;

struct test_data_t {
    W6522 *chip;
    cpu_state *cpu;
    InterruptController *ic;
};

class NClockIIt : public NClock {
    protected:
        bool slow_mode = false;
        void (*device_cycle)(void *data);
        void *device_data;

    public:
        NClockIIt(clock_set_t clock_set = CLOCK_SET_US, clock_mode_t clock_mode = CLOCK_1_024MHZ) : NClock(clock_set, clock_mode) {
            clock_mode = CLOCK_1_024MHZ;
        }
        void set_device_cycle(void (*device_cycle)(void *data), void *data) {
            this->device_cycle = device_cycle;
            this->device_data = data;
        }

        // II, II+, IIe
        /* inline virtual void slow_incr_cycles() override {
            cycles++; 
            c_14M += current.c_14M_per_cpu_cycle;
            
            if (video_scanner) {
                video_cycle_14M_count += current.c_14M_per_cpu_cycle;
                scanline_14M_count += current.c_14M_per_cpu_cycle;
    
                if (video_cycle_14M_count >= 14) {
                    video_cycle_14M_count -= 14;
                    //video_scanner->video_cycle();
                    video_cycles++;
                }
                if (scanline_14M_count >= 910) {  // end of scanline
                    c_14M += current.extra_per_scanline;
                    scanline_14M_count = 0;
                }
            }
        } */
        inline virtual void slow_incr_cycles() override {
            add_cpu();
            device_cycle(device_data);
        }

        inline void set_slow_mode(bool value) { slow_mode = value; }
        inline bool get_slow_mode() { return slow_mode; }
    
    
        virtual DebugFormatter *debug() override {
            DebugFormatter *f = NClock::debug();
            f->addLine("CPU Slow Mode: %d", get_slow_mode());
    
            return f;
        }
    
    };

struct reg_record_t {
    uint64_t cycle;
    uint8_t reg;
    uint8_t value;
};

reg_record_t recs[] = {
    {1, MB_6522_IER, 0b11000000}, // enable T1 interrupt (7:1 = "set", 6:1 = "set")
    {5, MB_6522_T1L_L, 0x05},
    {8, MB_6522_T1C_H, 0x00},
    {20, MB_6522_T1C_H, 0x00}, // write T1C-H, should clear interrupt pending and set up for another 1-shot

    {29, MB_6522_ACR, 0x40},
    {30, MB_6522_T1C_H, 0x00},
    {40, MB_6522_IFR, 0x40}, // clear T1 interrupt
    {60, MB_6522_IER,0b10100000}, // enable T2 interrupt also
    {61, MB_6522_T2L_L, 0x05},
    {62, MB_6522_T2C_H, 0x00},
};


void tickcycle(void *data) {
    test_data_t *td = (test_data_t *)data;
    W6522 *chip = td->chip;
    chip->debug_one();
    chip->incr_cycle();
    uint8_t ifr = chip->read(MB_6522_IFR);
    if (ifr & 0x80) {
        td->ic->assert_irq(IRQ_SLOT_1);
    } else {
        td->ic->deassert_irq(IRQ_SLOT_1);
    }
}

void write_c4(void *context, uint32_t address, uint8_t value) {
    test_data_t *td = (test_data_t *)context;
    W6522 *chip = td->chip;
    printf("write: Reg %02X <= %02X\n", address&0xFF, value);
    chip->write(address&0xFF, value);
}

int main(int argc, char *argv[]) {
    W6522 chip("6522 #1 $80");
    NClockIIt clock(CLOCK_SET_US, CLOCK_1_024MHZ);

    uint8_t *memory = (uint8_t *)malloc(256 * GS2_PAGE_SIZE);
    InterruptController ic;
    processor_type cpu_type = PROCESSOR_65C02;

    std::unique_ptr<BaseCPU> cpux = createCPU(cpu_type, &clock);
    cpu_state *cpu = new cpu_state(cpu_type);
    cpu->trace = true;

    MMU *mmu = new MMU(256, GS2_PAGE_SIZE);
    for (int i = 0; i < 256; i++) {
        mmu->map_page_both(i, &memory[i*256], "TEST RAM");
    }

    test_data_t td = { &chip, cpu, &ic };
    clock.set_device_cycle(tickcycle, &td);

    // map page c4 to 6522 write through an intermediary function.
    mmu->set_page_write_h(0xc4, {write_c4, &td}, "6522 write");

    cpu->set_mmu(mmu);
    ic.register_irq_receiver([cpu](bool irq_asserted) {
        cpu->irq_asserted = irq_asserted;
        printf("CPU IRQ asserted: %d\n", irq_asserted);
    });

    // load in our test program
    FILE *f = fopen("apps/mbtest/irqtimetest.bin", "rb");
    fread(memory+0x0300, 1, 0x60, f);
    fclose(f);
    cpu->pc = 0x0300;

    // set IRQ vector to code that does JMP ($03FE) (5C FE 03)
    mmu->write(0xFFEE, 0x6C);
    mmu->write(0xFFEF, 0xFE);
    mmu->write(0xFFF0, 0x03);
    mmu->write(0xFFFE, 0xEe);
    mmu->write(0xFFFF, 0xFF);
    mmu->write(0xFDDA, 0x60);
    mmu->write(0xFD8E, 0x60);

    int recindex = 0;
    uint64_t system_cycles = 0;
    
    while (1) {
        (cpux->execute_next)(cpu);
        if (cpu->trace) {
            char * trace_entry = cpu->trace_buffer->decode_trace_entry(&cpu->trace_entry);
            printf("%s\n", trace_entry);
        }
        /* if (recindex < sizeof(recs) / sizeof(recs[0])) {
            if (system_cycles >= recs[recindex].cycle) {
                chip.write(recs[recindex].reg, recs[recindex].value);
                printf("write: Reg %02X <= %02X\n", recs[recindex].reg, recs[recindex].value);
                recindex++;
            }
        } */
        
        //clock.incr_cycles();
        //chip.incr_cycle();
        system_cycles++;
    }
    return 0;
}