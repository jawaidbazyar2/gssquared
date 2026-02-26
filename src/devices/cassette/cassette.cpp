#include "gs2.hpp"
#include "computer.hpp"
#include "devices/cassette/cassette.hpp"

// This basically does nothing right now. Just return floating bus state.

uint8_t cassette_memory_read(void *context, uint32_t address) {
    cassette_state_t *cassette_state = (cassette_state_t *)context;

    return cassette_state->mmu->floating_bus_read() | 0x80; // "real apple //e enhanced with nothing plugged into cassette, reads 80 from C060" - Arekkusu
}

void init_mb_cassette(computer_t *computer, SlotType_t slot) {
    cassette_state_t *cassette_state = new cassette_state_t;
    cassette_state->mmu = computer->mmu;
    
    computer->mmu->set_C0XX_read_handler(0xC060, { cassette_memory_read, cassette_state });
    computer->mmu->set_C0XX_read_handler(0xC068, { cassette_memory_read, cassette_state });
}