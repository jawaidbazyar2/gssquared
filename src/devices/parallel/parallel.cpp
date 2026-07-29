#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "parallel.hpp"

#include "Device_ID.hpp"
#include "util/Connections.hpp"

void parallel_write_C0x0(void *context, uint32_t addr, uint8_t data) {
    parallel_data * parallel_d = (parallel_data *)context;
    if (DEBUG(DEBUG_PARALLEL)) {
        printf("parallel_write_C0x0 %x\n", data);
    }

    if (parallel_d->serial_device != nullptr) {
        parallel_d->serial_device->q_host.send(SerialMessage{MESSAGE_DATA, data});
    }
}

void parallel_reset(parallel_data *parallel_d) {
    if ((parallel_d != nullptr) && (parallel_d->id == DEVICE_ID_PARALLEL)) {
        if (parallel_d->serial_device != nullptr) {
            parallel_d->serial_device->q_host.send(SerialMessage{MESSAGE_CLOSE, 0});
        }
    }
}

void init_slot_parallel(computer_t *computer, SlotType_t slot) {
    parallel_data * parallel_d = new parallel_data;
    parallel_d->id = DEVICE_ID_PARALLEL;
    parallel_d->_slot = slot;

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

    snprintf(parallel_d->port_id, sizeof(parallel_d->port_id), "PARL%d", static_cast<int>(slot));

    char display_name[32];
    snprintf(display_name, sizeof(display_name), "Parallel Slot %d", static_cast<int>(slot));
    computer->connections->register_port(
        connection_key_t{static_cast<int>(slot), "a"},
        display_name,
        connection_port_kind_t::PARALLEL,
        connection_device_type_t::FILE,
        [parallel_d](SerialDevice *dev) {
            parallel_d->serial_device = dev;
        },
        parallel_d->port_id);

    computer->register_reset_handler(
        [parallel_d](bool cold_start) {
            (void)cold_start;
            parallel_reset(parallel_d);
            return true;
        });

    computer->register_shutdown_handler([parallel_d]() {
        // SerialDevice owned by Connections.
        parallel_d->serial_device = nullptr;
        delete parallel_d->rom;
        parallel_d->rom = nullptr;
        delete parallel_d;
        return true;
    });
}
