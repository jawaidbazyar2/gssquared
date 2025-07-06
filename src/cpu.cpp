/*
 *   Copyright (c) 2025 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cstdio>
#include <time.h>
#include <cstdlib>

#include "cpu.hpp"
#include "cpus/cpu_implementations.hpp"

clock_mode_info_t clock_mode_info[NUM_CLOCK_MODES] = {
    { 0, 0, 17008 },
    { 1020500, (1000000000 / 1020500)+1, 17008 },
    { 2800000, (1000000000 / 2800000), 46666 },
    { 4000000, (1000000000 / 4000000), 66665 }
};

void set_clock_mode(cpu_state *cpu, clock_mode_t mode) {
    // TODO: if this is ever called from inside a CPU loop, we need to exit that loop
    // immediately in order to avoid weird calculations around.
    // So add a "speedshift" cpu flag.

    cpu->HZ_RATE = clock_mode_info[mode].hz_rate;
    // Lookup time per emulated cycle
    cpu->cycle_duration_ns = clock_mode_info[mode].cycle_duration_ns;

    cpu->clock_mode = mode;
    fprintf(stdout, "Clock mode: %d HZ_RATE: %llu cycle_duration_ns: %llu \n", cpu->clock_mode, cpu->HZ_RATE, cpu->cycle_duration_ns);
}

void toggle_clock_mode(cpu_state *cpu) {
    set_clock_mode(cpu, (clock_mode_t)((cpu->clock_mode + 1) % NUM_CLOCK_MODES));
    fprintf(stdout, "Clock mode: %d HZ_RATE: %llu\n", cpu->clock_mode, cpu->HZ_RATE);
}

#if 0
processor_model processor_models[NUM_PROCESSOR_TYPES] = {
    { "6502 (nmos)", cpu_6502::execute_next },
    //{ "65C02 (cmos)", cpu_65c02::execute_next }
    { "65C02 (cmos)", cpu_6502::execute_next }
};

const char* processor_get_name(int processor_type) {
    return processor_models[processor_type].name;
}
#endif

/** State storage for non-slot devices. */
void *get_module_state(cpu_state *cpu, module_id_t module_id) {
    void *state = cpu->module_store[module_id];
    if (state == nullptr) {
        fprintf(stderr, "Module %d not initialized\n", module_id);
    }
    return state;
}

void set_module_state(cpu_state *cpu, module_id_t module_id, void *state) {
    cpu->module_store[module_id] = state;
}

/** State storage for slot devices. */
SlotData *get_slot_state(cpu_state *cpu, SlotType_t slot) {
    SlotData *state = cpu->slot_store[slot];
    /* if (state == nullptr) {
        fprintf(stderr, "Slot Data for slot %d not initialized\n", slot);
    } */
    return state;
}

SlotData *get_slot_state_by_id(cpu_state *cpu, device_id id) {
    for (int i = 0; i < 8; i++) {
        if (cpu->slot_store[i] && cpu->slot_store[i]->id == id) {
            return cpu->slot_store[i];
        }
    }
    return nullptr;
}

void set_slot_state(cpu_state *cpu, SlotType_t slot, /* void */ SlotData *state) {
    state->_slot = slot;
    cpu->slot_store[slot] = state;
}

void set_slot_irq(cpu_state *cpu, uint8_t slot, bool irq) {
    if (irq) {
        cpu->irq_asserted |= (1 << slot);
    } else {
        cpu->irq_asserted &= ~(1 << slot);
    }
}

cpu_state::cpu_state() {
    pc = 0x0400;
    sp = rand() & 0xFF; // simulate a random stack pointer
    a = 0;
    x = 0;
    y = 0;
    p = 0;
    cycles = 0;
    last_tick = 0;
    
    trace = true;
    trace_buffer = new system_trace_buffer(100000);

    set_clock_mode(this, CLOCK_1_024MHZ);

    // initialize these things
    for (int i = 0; i < NUM_SLOTS; i++) {
        slot_store[i] = nullptr;
    }
    for (int i = 0; i < MODULE_NUM_MODULES; i++) {
        module_store[i] = nullptr;
    }
}

#if 0
void cpu_state::set_processor(int processor_type) {
    execute_next = processor_models[processor_type].execute_next;
}
#endif

void cpu_state::reset() {
    halt = 0; // if we were STPed etc.
    I = 1; // set interrupt flag.
    pc = read_word(RESET_VECTOR);
}

cpu_state::~cpu_state() {
    if (trace_buffer != nullptr) {
        delete trace_buffer;
    }
}