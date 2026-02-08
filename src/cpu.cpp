/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

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

cpu_state::cpu_state(processor_type cpu_type) {
    full_db = 0;
    full_pc = 0; // was 0x400 from original tests, ha!
    sp = rand() & 0xFF; // simulate a random stack pointer
    a = 0;
    x = 0;
    y = 0;
    p = 0;
    d = 0;
    /* cycles = 0; */ // moved to clock.
    
    trace = true;
    trace_buffer = new system_trace_buffer(100000, cpu_type);

    /* set_clock_mode(this, CLOCK_1_024MHZ); */

    // initialize these things
    for (int i = 0; i < NUM_SLOTS; i++) {
        slot_store[i] = nullptr;
    }
    for (int i = 0; i < MODULE_NUM_MODULES; i++) {
        module_store[i] = nullptr;
    }
}

void cpu_state::set_processor(processor_type new_cpu_type) {
    cpu_type = new_cpu_type;
    trace_buffer->set_cpu_type(new_cpu_type);
}

void cpu_state::reset() {
    halt = 0; // if we were STPed etc.
    I = 1; // set interrupt flag.
    //skip_next_irq_check = 0;
    ICHANGE = false;
    EFFI = 0;
    //pc = read_word(RESET_VECTOR);
    core->reset(this);
}

cpu_state::~cpu_state() {
    if (trace_buffer != nullptr) {
        delete trace_buffer;
    }
}