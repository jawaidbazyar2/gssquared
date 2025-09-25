#include "SDL3/SDL_events.h"
#include "gs2.hpp"
#include "cpu.hpp"

#include "mouse.hpp"
#include "computer.hpp"
#include "debug.hpp"


void mouse_propagate_interrupt(mouse_state_t *ds) {
    // for each chip, calculate the IFR bit 7.
    uint8_t irqmodes = (ds->mode.value & MOUSE_INT_MASK) & ds->status.value;
    bool irq_to_slot = (irqmodes != 0);

    //printf("irq_to_slot: %d %d\n", mb_d->slot, irq_to_slot);
    ds->computer->set_slot_irq(ds->_slot, irq_to_slot);
}

void mouse_reset(mouse_state_t *ds) {
    ds->x_pos.value = 0;
    ds->y_pos.value = 0;
    ds->x_clamp_low.value = 0;
    ds->x_clamp_high.value = 1023;
    ds->y_clamp_low.value = 0;
    ds->y_clamp_high.value = 1023;
    ds->status.value = 0;
    ds->mode.value = 0;
    ds->button_last_read = 0;

    ds->last_x_pos = 0;
    ds->last_y_pos = 0;
}

uint8_t mouse_read_c0xx(void *context, uint16_t address) {
    mouse_state_t *ds = (mouse_state_t *)context;
    uint8_t val;
    uint8_t addr = address & 0x0F;

    switch (addr) {
        case 0x00:
            val = ds->x_pos.l; break;
        case 0x01:
            val = ds->x_pos.h; break;
        case 0x02:
            val = ds->y_pos.l; break;
        case 0x03:
            val = ds->y_pos.h; break;
        case 0x04:
            val =  ds->x_clamp_low.l; break;
        case 0x05:
            val =  ds->x_clamp_low.h; break;
        case 0x06:
            val =  ds->x_clamp_high.l; break;
        case 0x07:
            val =  ds->x_clamp_high.h; break;
        case 0x08:
            val =  ds->y_clamp_low.l; break;
        case 0x09:
            val =  ds->y_clamp_low.h; break;
        case 0x0A:
            val =  ds->y_clamp_high.l; break;
        case 0x0B:
            val =  ds->y_clamp_high.h; break;
        case 0x0E: { // try clearing IRQ flags after we get the status value.
                ds->status.last_button_down = ds->button_last_read; // this is what the button was the last time it was read.
                ds->button_last_read = ds->status.button_down; // this is what the button is now.
                uint8_t tmp_status = ds->status.value;
                ds->status.int_vbl = 0;
                ds->status.int_motion = 0;
                ds->status.int_button = 0;
                ds->status.x_y_changed = 0;
                mouse_propagate_interrupt(ds);
                val = tmp_status;
            } break;
        case 0x0F:
            val = ds->mode.value; break;
        default:
            val = 0xEE; break;
    }
    //printf("Mouse read: %02X: %02X\n", address, val);
    return val;
}


void clamp_mouse(mouse_state_t *ds, int &x, int &y) {
    if (x < ds->x_clamp_low.value) {
        x = ds->x_clamp_low.value;
    }
    if (x > ds->x_clamp_high.value) {
        x = ds->x_clamp_high.value;
    }
    if (y < ds->y_clamp_low.value) {
        y = ds->y_clamp_low.value;
    }
    if (y > ds->y_clamp_high.value) {
        y = ds->y_clamp_high.value;
    }
}

void mouse_write_c0xx(void *context, uint16_t address, uint8_t value) {
    mouse_state_t *ds = (mouse_state_t *)context;
    uint8_t addr = address & 0x0F;
    switch (addr) {
        case 0x00:
            ds->x_pos.l = value;
            break;
        case 0x01:
            ds->x_pos.h = value;
            break;
        case 0x02:
            ds->y_pos.l = value;
            break;
        case 0x03:
            ds->y_pos.h = value;
            break;
        case 0x04:
            ds->x_clamp_low.l = value;
            break;
        case 0x05:
            ds->x_clamp_low.h = value;
            break;
        case 0x06:
            ds->x_clamp_high.l = value;
            break;
        case 0x07:
            ds->x_clamp_high.h = value;
            break;
        case 0x08:
            ds->y_clamp_low.l = value;
            break;
        case 0x09:
            ds->y_clamp_low.h = value;
            break;
        case 0x0A:
            ds->y_clamp_high.l = value;
            break;
        case 0x0B:
            ds->y_clamp_high.h = value;
            break;
        case 0x0E:
            ds->status.value = value;
            break;
        case 0x0F:
            ds->mode.value = value;
            break;
        default:
            break;
    }
    //printf("Mouse write: %02X: %02X\n", address, value);

    // re-clamp mouse based on any changes to values above.
    int tmp_x = ds->x_pos.value;
    int tmp_y = ds->y_pos.value;
    clamp_mouse(ds, tmp_x, tmp_y);
    ds->x_pos.value = tmp_x;
    ds->y_pos.value = tmp_y;
}

bool mouse_motion(mouse_state_t *ds, const SDL_Event &event) {
    if (event.type != SDL_EVENT_MOUSE_MOTION) {
        return false;
    }
    int motion_x = event.motion.xrel;
    int motion_y = event.motion.yrel;
    if (motion_x == 0 && motion_y == 0) {
        return false;
    }
    // by definition the mouse will have moved in here.
    ds->motion_x = motion_x;
    ds->motion_y = motion_y;

    int tmp_x = ds->x_pos.value + motion_x;
    int tmp_y = ds->y_pos.value + motion_y;
    clamp_mouse(ds, tmp_x, tmp_y);
    ds->x_pos.value = tmp_x;
    ds->y_pos.value = tmp_y;
    
/*     if ((ds->last_x_pos == ds->x_pos.value) && (ds->last_y_pos == ds->y_pos.value)) {
        ds->status.x_y_changed = 0;
    } else if ((ds->last_x_pos != ds->x_pos.value) || (ds->last_y_pos != ds->y_pos.value)) { */
        ds->status.x_y_changed = 1;
        ds->last_x_pos = ds->x_pos.value;
        ds->last_y_pos = ds->y_pos.value;
        if (ds->mode.int_ena_motion) {
            ds->status.int_motion = 1;
            mouse_propagate_interrupt(ds);
        }
 /*    } */
     //printf("motion_x: %d, motion_y: %d status: %02X\n", motion_x, motion_y, ds->status.value);

    return true;
}

// track mouse button status
bool mouse_updown(mouse_state_t *ds, const SDL_Event &event) {
    // set button status bit based on mouse event.
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        ds->status.button_down = 1;
        if (ds->mode.int_ena_button) {
            ds->status.int_button = 1;
            mouse_propagate_interrupt(ds);
        }
        if (DEBUG(DEBUG_MOUSE)) fprintf(stdout, "Mouse button down: %02X\n", ds->status.value);
        return true;
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        ds->status.button_down = 0;
        if (DEBUG(DEBUG_MOUSE)) fprintf(stdout, "Mouse button up: %02X\n", ds->status.value);
        return true;
    }
    return false;
}

void mouse_vbl_interrupt(uint64_t instanceID, void *user_data) {
    mouse_state_t *ds = (mouse_state_t *)user_data;
    //ds->vbl_cycle += 17030;
    
    // calc based on cycle at start of frame, 
    // number of cycles in a frame divided by 262 times 192nd scanline (vbl area)
    //ds->vbl_cycle = ds->computer->get_frame_start_cycle() + (ds->computer->cpu->cycles_per_scanline * 192);
    // current frame - plus cycle per frame (next frame) at scanline 192.
    // going from ludicrous (or likely any higher speed to a lower speed). 
    ds->vbl_cycle = ds->computer->get_frame_start_cycle() + ds->computer->cpu->cycles_per_frame  + ds->computer->cpu->cycles_per_scanline * 192;
    
    ds->status.int_vbl = 1;
    mouse_propagate_interrupt(ds);
    if (ds->vbl_cycle <= ds->computer->cpu->cycles) {
        fprintf(stdout, "Mouse vbl cycle is before current cycle: %llu < %llu\n", ds->vbl_cycle, ds->computer->cpu->cycles);
        return;
    }
    ds->event_timer->scheduleEvent(ds->vbl_cycle, mouse_vbl_interrupt, instanceID, ds);
}

DebugFormatter * debug_mouse(mouse_state_t *ds) {
    DebugFormatter *df = new DebugFormatter();
        
    df->addLine("   Mouse ");
    df->addLine("  X: %6d Y: %6d", ds->x_pos.value, ds->y_pos.value);
    df->addLine("  X Clamp Low: %6d X Clamp High: %6d", ds->x_clamp_low.value, ds->x_clamp_high.value);
    df->addLine("  Y Clamp Low: %6d Y Clamp High: %6d", ds->y_clamp_low.value, ds->y_clamp_high.value);
    df->addLine("  Status: %02X", ds->status.value);
    df->addLine("  Mode: %02X", ds->mode.value);
    df->addLine("  VBL Offset %6d", ds->vbl_offset);
    df->addLine("  Motion X: %6d  Y: %6d", ds->motion_x, ds->motion_y);
    return df;
}

void init_mouse(computer_t *computer, SlotType_t slot) {

    // alloc and init display state
    mouse_state_t *ds = new mouse_state_t;
    ds->id = DEVICE_ID_MOUSE;
    ds->computer = computer;
    ds->event_timer = computer->event_timer;
    mouse_reset(ds);
    //ds->vbl_cycle = 12480;
    ds->vbl_cycle = (ds->computer->cpu->cycles_per_scanline * 192);
    ds->vbl_offset = ds->vbl_cycle;

    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,"1");

    // set in CPU so we can reference later
    set_slot_state(computer->cpu, slot, (SlotData *)ds);

    ResourceFile *rom = new ResourceFile("roms/cards/mouse/mouse.rom", READ_ONLY);
    if (rom == nullptr) {
        std::cerr << "Failed to load pdblock2.rom" << std::endl;
        return;
    }
    rom->load();
    ds->rom = (uint8_t *)(rom->get_data());
    computer->mmu->set_slot_rom(slot, ds->rom, "MOUSE_ROM");

    uint16_t slot_address = 0xC080 + (slot * 0x10);
    // set in CPU so we can reference later
    if (DEBUG(DEBUG_MOUSE)) fprintf(stdout, "Initializing mouse\n");

    // register the I/O ports
    for (int i = 0; i < 0x0C; i++) {
        computer->mmu->set_C0XX_read_handler(slot_address + i, { mouse_read_c0xx, ds });
        computer->mmu->set_C0XX_write_handler(slot_address + i, { mouse_write_c0xx, ds });
    }
    for (int i = 0x0E; i < 0x10; i++) {
        computer->mmu->set_C0XX_read_handler(slot_address + i, { mouse_read_c0xx, ds });
        computer->mmu->set_C0XX_write_handler(slot_address + i, { mouse_write_c0xx, ds });
    }

    computer->register_reset_handler(
        [ds]() {
            mouse_reset(ds);
            return true;
        });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_MOTION, 
        [ds](const SDL_Event &event) {
            return mouse_motion(ds, event);
            //return true;
        });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, 
        [ds](const SDL_Event &event) {
            mouse_updown(ds, event);
            return true;
        });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_UP, 
        [ds](const SDL_Event &event) {
            mouse_updown(ds, event);
            return true;
        });
    computer->dispatch->registerHandler(SDL_EVENT_KEY_DOWN,
        [ds](const SDL_Event &event) {
            if (event.key.key == SDLK_KP_DIVIDE) {
                if (ds->vbl_offset >= 500) {
                    ds->vbl_cycle -= 500;
                    ds->vbl_offset -= 500;
                }
                printf("vbl_cycle: %llu, vbl_offset: %llu\n", ds->vbl_cycle, ds->vbl_offset);
            }
            if (event.key.key == SDLK_KP_MULTIPLY) {
                ds->vbl_cycle += 500;
                ds->vbl_offset += 500;
                printf("vbl_cycle: %llu, vbl_offset: %llu\n", ds->vbl_cycle, ds->vbl_offset);
            }
            return true;
        }
    );

    computer->register_debug_display_handler(
        "mouse",
        0x0000000000000002, // unique ID for this, need to have in a header.
        [ds]() -> DebugFormatter * {
            return debug_mouse(ds);
        }
    );
    
    // schedule timer for vbl to start during vbl of next frame.
    //ds->event_timer->scheduleEvent(ds->vbl_cycle + 17030, mouse_vbl_interrupt, 0x10000000 | (slot << 8) | 0, ds);
    ds->event_timer->scheduleEvent(computer->get_frame_start_cycle() + ds->vbl_offset, mouse_vbl_interrupt, 0x10000000 | (slot << 8) | 0, ds);


    if (DEBUG(DEBUG_MOUSE)) fprintf(stdout, "Mouse initialized\n");
}
