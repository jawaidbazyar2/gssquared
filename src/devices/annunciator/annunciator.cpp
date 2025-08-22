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


#include <SDL3/SDL.h>
#include "gs2.hpp"
#include "cpu.hpp"
#include "debug.hpp"
#include "devices/annunciator/annunciator.hpp"

uint8_t read_annunciator(cpu_state *cpu, uint8_t id) {
    annunciator_state_t * anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
    return anc_d->annunciators[id];
}

uint8_t annunciator_read_C0xx_anc0(void *context, uint16_t addr) {
    cpu_state *cpu = (cpu_state *)context;
    annunciator_state_t * anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
    uint8_t anc_id = (addr & 0x7) >> 1;
    uint8_t anc_state = (addr & 0x1);
    anc_d->annunciators[anc_id] = anc_state;
    if (DEBUG(DEBUG_VIDEX)) fprintf(stdout, "videx_read_C0xx_anc0: %04X %d\n", addr, anc_d->annunciators[anc_id]);
    return 0xEE; // TODO: return floating bus.
}

void annunciator_write_C0xx_anc0(void *context, uint16_t addr, uint8_t data) {
    cpu_state *cpu = (cpu_state *)context;
    annunciator_state_t * anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
    uint8_t anc_id = (addr & 0x7) >> 1;
    uint8_t anc_state = (addr & 0x1);
    anc_d->annunciators[anc_id] = anc_state;
    if (DEBUG(DEBUG_VIDEX)) fprintf(stdout, "videx_write_C0xx_anc0: %04X %d\n", addr, anc_d->annunciators[anc_id]);
}

void init_annunciator(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    SDL_InitSubSystem(SDL_INIT_JOYSTICK); // TODO: I don't think this belongs here?
    // alloc and init display state
    annunciator_state_t *anc_d = new annunciator_state_t;
    anc_d->annunciators[0] = 0;
    anc_d->annunciators[1] = 0;
    anc_d->annunciators[2] = 0;
    anc_d->annunciators[3] = 0;
    
    // set in CPU so we can reference later
    set_module_state(cpu, MODULE_ANNUNCIATOR, anc_d);

    if (DEBUG(DEBUG_GAME)) fprintf(stdout, "Initializing annunciator\n");

    for (int i = 0; i < 3; i++) { // we'll handle annunciators 0-2 here. display will handle 3.
        computer->mmu->set_C0XX_read_handler(0xC058 + i*2, { annunciator_read_C0xx_anc0, cpu });
        computer->mmu->set_C0XX_read_handler(0xC058 + i*2 + 1, { annunciator_read_C0xx_anc0, cpu });
        computer->mmu->set_C0XX_write_handler(0xC058 + i*2, { annunciator_write_C0xx_anc0, cpu });
        computer->mmu->set_C0XX_write_handler(0xC058 + i*2 + 1, { annunciator_write_C0xx_anc0, cpu });
    }

}
