#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "parallel.hpp"

void parallel_write_C0x0(void *context, uint32_t addr, uint8_t data) {
    parallel_data * parallel_d = (parallel_data *)context;
    if (DEBUG(DEBUG_PARALLEL)) {
        printf("parallel_write_C0x0 %x\n", data);
    }

    // TODO: use a unique filename for each print job.
    if (parallel_d->output == nullptr) {
        parallel_d->output = fopen("parallel.out", "a");
    }
    fputc(data, parallel_d->output);
}

void parallel_reset(parallel_data *parallel_d) {
    if ((parallel_d != nullptr) && (parallel_d->id == DEVICE_ID_PARALLEL)) {
        if (parallel_d->output != nullptr) {
            fclose(parallel_d->output);
            parallel_d->output = nullptr;
        }
    }
}

void init_slot_parallel(computer_t *computer, SlotType_t slot) {
    parallel_data * parallel_d = new parallel_data;
    parallel_d->id = DEVICE_ID_PARALLEL;
    
    ResourceFile *rom = new ResourceFile("roms/cards/parallel/parallel.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "Failed to load parallel.rom\n");
        return;
    }
    rom->load();
    parallel_d->rom = rom;

    fprintf(stdout, "init_slot_parallel %d\n", slot);

    uint16_t slot_base = 0xC080 + (slot * 0x10);

    computer->mmu->set_C0XX_write_handler(slot_base + PARALLEL_DEV, { parallel_write_C0x0, parallel_d });
    
    uint8_t *rom_data = parallel_d->rom->get_data();
    computer->mmu->set_slot_rom(slot, rom_data, "PARL_ROM");

    // TODO: register a frame handler to track automatic timeout and closing of output file.

    computer->register_reset_handler(
        [parallel_d]() {
            parallel_reset(parallel_d);
            return true;
        });
}