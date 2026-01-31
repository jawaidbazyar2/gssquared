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

clock_mode_info_t us_clock_mode_info[NUM_CLOCK_MODES] = {
    { 14'318'180, 14318180, 238420, 1, 0, 912, 238944, 16688154, 16688155/* , 1020484 */ }, // cycle times here are fake.
    { 1'020'484, 14318180, 17030, 14, 2, 65, 238944, 16688154, 16688155/* , 1020484 */ },
    { 2'857'370, 14318180,  47684, 5, 2, 182, 238944, 16688154, 16688155/* , 2857356  */},
    { 7'143'390, 14318180, 119210, 2, 2, 455, 238944, 16688154, 16688155/* , 7143390 */ },
    { 14'286'780, 14318180, 238420, 1, 2, 912, 238944, 16688154, 16688155/* , 14286780 */ }
};

clock_mode_info_t pal_clock_mode_info[NUM_CLOCK_MODES] = {
    { 14'250'450, 14250450, 283920, 1, 0, 912, 284544, 19967369, 19967370/* , 1'015'657 */ }, // cycle times here are fake.
    { 1'015'657, 14250450,  20280, 14, 2, 65, 284544, 19967369, 19967370/* , 1'015'657 */ },
    { 2'857'370, 14250450,  56784, 5, 2, 182, 284544, 19967369, 19967370/* , 2'843'839 */ },
    { 7'143'390,14250450,  141960, 2, 2, 455, 284544, 19967369, 19967370/* , 7'109'599 */ },
    { 14'250'450, 14250450, 283920, 1, 2, 910, 284544, 19967369, 19967370/* , 14'219'199 */ }
    //     { 14'250'450,14250450,  284232, 1, 0, 912, 284544, 19967369, 19967370, 14'219'199 } // it was this which didn't work..
};

clock_mode_info_t *system_clock_mode_info = us_clock_mode_info;

void select_system_clock(clock_set_t clock_set) {
    if (clock_set == CLOCK_SET_US) {
        system_clock_mode_info = us_clock_mode_info;
    } else if (clock_set == CLOCK_SET_PAL) {
        system_clock_mode_info = pal_clock_mode_info;
    }
}

void set_clock_mode(cpu_state *cpu, clock_mode_t mode) {
    // TODO: copy the entire struct into the cpu state?
    cpu->HZ_RATE = system_clock_mode_info[mode].hz_rate;
    // Lookup time per emulated cycle
    //cpu->cycle_duration_ns = clock_mode_info[mode].cycle_duration_ns;
    cpu->c_14M_per_cpu_cycle = system_clock_mode_info[mode].c_14M_per_cpu_cycle;
    cpu->cycles_per_scanline = system_clock_mode_info[mode].cycles_per_scanline;
    cpu->extra_per_scanline = system_clock_mode_info[mode].extra_per_scanline;
    cpu->cycles_per_frame = system_clock_mode_info[mode].cycles_per_frame;

    cpu->clock_mode = mode;

    //fprintf(stdout, "Clock mode: %d HZ_RATE: %llu \n", cpu->clock_mode, cpu->HZ_RATE);
}

clock_mode_info_t *get_clock_line(cpu_state *cpu) {
    return &system_clock_mode_info[cpu->clock_mode];
}

clock_mode_t toggle_clock_mode(cpu_state *cpu, int direction) {
    int new_mode = (int)cpu->clock_mode + direction;
    
    if (new_mode < 0) {
        new_mode = (NUM_CLOCK_MODES - 1);
    } else if (new_mode >= NUM_CLOCK_MODES) {
        new_mode = 0;
    }
    fprintf(stdout, "Clock mode: %d\n", new_mode);
    return (clock_mode_t)new_mode;
}

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

void set_device_irq(cpu_state *cpu, device_irq_id devid, bool irq) {
    if (irq) {
        cpu->irq_asserted |= (1 << devid);
    } else {
        cpu->irq_asserted &= ~(1 << devid);
    }
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
    cycles = 0;
    //last_tick = 0;
    
    trace = true;
    trace_buffer = new system_trace_buffer(100000, cpu_type);

    set_clock_mode(this, CLOCK_1_024MHZ);

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
    skip_next_irq_check = 0;
    //pc = read_word(RESET_VECTOR);
    core->reset(this);
}

cpu_state::~cpu_state() {
    if (trace_buffer != nullptr) {
        delete trace_buffer;
    }
}