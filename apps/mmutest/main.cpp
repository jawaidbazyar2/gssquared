#include <SDL3/SDL.h>

#include "mmus/mmu.hpp"
#include "mmus/mmu_ii.hpp"
#include "util/printf_helper.hpp"

/* With optimizations on, this test takes about 1ns per byte read/written from emulated memory */

#define MEM_SIZE 48*1024
#define PAGE_SIZE 256

struct fake_slot_context_t {
    MMU_II *mmu;
    uint8_t *card_rom;
};

struct fake_io_context_t {
    MMU_II *mmu;
    uint8_t *io_ram;
};

struct fake_mb_context_t {
    MMU_II *mmu;
    uint8_t more_mb_config;  
};

struct text_page_context_t {
    MMU_II *mmu;
    uint8_t *text_page_ram;
};

void fake_videx_c8xx(void *context, SlotType_t slot) {
    printf("C8xx handler called for slot %d\n", slot);
    fake_slot_context_t *ctx = (fake_slot_context_t *)context;
    for (int i = 0; i < 8; i++) {
        ctx->mmu->map_page_read_only(i + 0xC8, ctx->card_rom + i * PAGE_SIZE, "VIDEXFAK");
    }
}

uint8_t fake_C000_read_handler(void *context, uint32_t address) {
    printf("C000 handler called for address %04X\n", address);
    return 0;
};

uint8_t fake_mb_CSXX_read_handler(void *context, uint32_t address) {
    printf("CSXX MB read handler called for address %04X\n", address);
    return 0;
};

void fake_mb_CSXX_write_handler(void *context, uint32_t address, uint8_t value) {
    printf("CSXX MB write handler called for address %04X: %02X\n", address, value);
    return;
};

void fake_text_page_write_handler(void *context, uint32_t address, uint8_t value) {
    //printf("T");
    // calculate modified line here. Verified we are indeed being called.
    return;
};


int main(int argc, char **argv) {
    MMU_II mmu(MEM_SIZE / PAGE_SIZE, MEM_SIZE, nullptr);
    
    uint8_t *ram = new uint8_t[MEM_SIZE];
    for (int page = 0; page < MEM_SIZE / PAGE_SIZE; page++) {
        mmu.map_page_both(page, ram + page * PAGE_SIZE, "TEST RAM");
    }

    // Set up fake Slot 3 videx device
    // this is how a slot card will register itself with the MMU
    fake_slot_context_t fake_videx = { &mmu, nullptr };
    fake_videx.card_rom = new uint8_t[2048];
    uint8_t *cdrom = new uint8_t[256];
    mmu.set_slot_rom(SLOT_3, cdrom, "VIDEXROM");
    mmu.set_C8xx_handler(SLOT_3, fake_videx_c8xx, &fake_videx);

    // This is how a mockingboard will set itself up to take all reads and writes as handler routines.
    fake_mb_context_t fake_mbconfig = { &mmu, 0 };
    read_handler_t fk2 = { fake_mb_CSXX_read_handler, &fake_mbconfig };
    write_handler_t fkw2 = { fake_mb_CSXX_write_handler, &fake_mbconfig };
    mmu.set_page_read_h(0xC4, fk2, "MB_CSXX");
    mmu.set_page_write_h(0xC4, fkw2, "MB_CSXX");

    // Set up fake C000 read handler
    // This is how various I/O devices with softswitches in C000 - C0FF register with the MMU.
    fake_io_context_t c000_ctx = { &mmu, nullptr };
    read_handler_t fk = { fake_C000_read_handler, &c000_ctx };
    mmu.set_C0XX_read_handler(0xC000, fk);

    // add a test for text/hgr shadowing functions.
    // this is how we shadow text/hgr pages into a handler function, to calculate which display "lines" have been modified.
    text_page_context_t tp0400 = { &mmu, nullptr };
    text_page_context_t tp0800 = { &mmu, nullptr };
    write_handler_t th0400 = { fake_text_page_write_handler, &tp0400 };
    write_handler_t th0800 = { fake_text_page_write_handler, &tp0800 };
    mmu.set_page_shadow(0x04, th0400);

    mmu.dump_page_table(0, 255);

    // main performance testing loop
    uint64_t totalbytes = 0;
    uint64_t start_time = SDL_GetTicksNS();    
    for (int i = 0; i < 10000; i++) {
        for (uint16_t addr = 0; addr < MEM_SIZE; addr++) {
            mmu.write(addr, addr & 0xFF);
            totalbytes++;
            uint8_t val = mmu.read(addr);
            totalbytes++;
            if (val != (addr & 0xFF)) {
                printf("Read back value mismatch at address %04X: %02X != %02X\n", addr, val, i & 0xFF);
                exit(0);
            }
        }
    }
    uint64_t end_time = SDL_GetTicksNS();

    // try flipping a few "soft switches" and reviewing results
    mmu.read(0xC4FF);
    mmu.dump_page_table(0xC8, 0xCF);

    mmu.read(0xCFFF);
    mmu.dump_page_table(0xC0, 0xCF);

    mmu.read(0xC000);
    mmu.dump_C0XX_handlers();

    mmu.write(0xC420, 0x55);

    // Get the memory address where page XX starts - used by video logic to directly read memory w/o going through MMU
    uint8_t *hgrbase = mmu.get_page_base_address(0x20);
    printf("HGR base address: %p\n", hgrbase);

    printf("Time taken: %llu ns\n", u64_t(end_time - start_time));

    printf("total bytes read/written: %llu\n", u64_t(totalbytes));

    float ns_per_byte = (float)(end_time - start_time) / totalbytes;    
    printf("ns per byte read/written avg: %f\n", ns_per_byte);
    printf("maximum simulated clock rate w/memory access every cycle: %f MHz\n", 1000.0f / ns_per_byte);

    mmu.dump_page(0x35);

    return 0;
}


int main2(int argc, char **argv) {
    MMU mmu(MEM_SIZE / PAGE_SIZE, PAGE_SIZE);
    
    uint8_t *ram = new uint8_t[MEM_SIZE];
    for (int page = 0; page < MEM_SIZE / PAGE_SIZE; page++) {
        mmu.map_page_both(page, ram + page * PAGE_SIZE, "TEST RAM");
    }

    mmu.dump_page_table(0, 191);

    // main performance testing loop
    uint64_t totalbytes = 0;
    uint64_t start_time = SDL_GetTicksNS();    
    for (int i = 0; i < 10000; i++) {
        for (uint16_t addr = 0; addr < MEM_SIZE; addr++) {
            mmu.write(addr, addr & 0xFF);
            totalbytes++;
            uint8_t val = mmu.read(addr);
            totalbytes++;
            if (val != (addr & 0xFF)) {
                printf("Read back value mismatch at address %04X: %02X != %02X\n", addr, val, i & 0xFF);
                exit(0);
            }
        }
    }
    uint64_t end_time = SDL_GetTicksNS();

    // Get the memory address where page XX starts - used by video logic to directly read memory w/o going through MMU
    uint8_t *hgrbase = mmu.get_page_base_address(0x20);
    printf("HGR base address: %p\n", hgrbase);

    printf("Time taken: %llu ns\n", u64_t(end_time - start_time));

    printf("total bytes read/written: %llu\n", u64_t(totalbytes));

    float ns_per_byte = (float)(end_time - start_time) / totalbytes;    
    printf("ns per byte read/written avg: %f\n", ns_per_byte);
    printf("maximum simulated clock rate w/memory access every cycle: %f MHz\n", 1000.0f / ns_per_byte);

    mmu.dump_page(0x35);

    return 0;
}