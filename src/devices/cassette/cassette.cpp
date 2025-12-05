#include "gs2.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "devices/cassette/cassette.hpp"

// This basically does nothing right now. Just return floating bus state.

uint8_t cassette_memory_read(void *context, uint32_t address) {
    cpu_state *cpu = (cpu_state *)context;
    return cpu->mmu->floating_bus_read() & 0x7F;
}


void init_mb_cassette(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    //set_module_state(cpu, MODULE_CASSETTE, cassette_state);

    computer->mmu->set_C0XX_read_handler(0xC060, { cassette_memory_read, cpu });
    computer->mmu->set_C0XX_read_handler(0xC068, { cassette_memory_read, cpu });
}