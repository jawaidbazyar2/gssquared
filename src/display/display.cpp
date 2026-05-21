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

#include <SDL3/SDL.h>

#include "SDL3/SDL_render.h"
#include "SDL3/SDL_surface.h"
#include "cpu.hpp"
#include "gs2.hpp"
#include "debug.hpp"

#include "display.hpp"
#include "platforms.hpp"

#include "util/dialog.hpp"

#include "display/ntsc.hpp"

#include "videosystem.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/VideoScanGenerator.hpp"
#include "devices/displaypp/VideoScanGenerator_Intf.hpp"
#include "devices/displaypp/VideoScanGenerator_RGB.hpp"
#include "devices/displaypp/VideoScanGenerator_Comp.hpp"
#include "mbus/MessageBus.hpp"
#include "mbus/KeyboardMessage.hpp"
#include "util/EventTimer.hpp"

#include "devices/displaypp/VideoScanGenerator.cpp"
#include "devices/displaypp/VideoScannerIIgs.hpp"
#include "devices/displaypp/VideoScannerIIe.hpp"
#include "devices/displaypp/VideoScannerIIePAL.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/AppleIIgsColors.hpp"

#include "util/DebugHandlerIDs.hpp"

/*
Index by scanner
*/
constexpr SDL_FRect content_rec_vsg2[4][2] = {
    //{ { 84.0, 12.0, 560+168, 240.0 }, { 0.0, 0.0,0.0,0.0 } }, // no shr here
    //{ { 168.0, 34.0, 560, 192.0 }, { 0.0, 0.0,0.0,0.0 } }, // this is exactly our bounding box.
    
    // II / II+
    { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+21 }, { 0.0, 0.0,0.0,0.0 } },
    
    // IIe - is slightly (7 pixels) wider.
    { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+21 }, { 0.0, 0.0,0.0,0.0 } },
    //{ { 168.0-42-7, 34.0-20, 560+42+42, 192.0+20+20 }, { 0.0, 0.0,0.0,0.0 } },  // use something like this later for composite vsg

    // IIePAL - is slightly (7 pixels) wider.
    { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+21 }, { 0.0, 0.0,0.0,0.0 } },

    // IIgs            
    // { { 0.0, 0.0, 910, 263.0 }, { 0.0, 2.0, 1040, 263.0 } }, // entire content area
    { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+29 }, { 192.0-48.0, 35.0-19.0, 640.0+48+48, 200+19+21.0 } },
};

bool update_display_apple2_cycle(display_state_t *ds) {
    video_system_t *vs = ds->video_system;

    ScanBuffer *scanbuf = ds->video_scanner->get_frame_scan();

    // TODO: This stuff takes basically no time, but it might make more sense to encap this in a helper routine somewhere else

    switch (vs->display_color_engine) {
        case DM_ENGINE_MONO:
            ds->vsg = ds->vsgc;
            ds->mon_mono.set_mono_color(vs->get_mono_color());
            ds->vsg->set_render(&ds->mon_mono);
            break;
        case DM_ENGINE_NTSC:
            ds->vsg = ds->vsgc;
            ds->mon_mono.set_mono_color(RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)); // white
            ds->vsg->set_render(&ds->mon_ntsc);
            break;
        case DM_ENGINE_RGB:
            ds->vsg = ds->vsgr;
            break;
        default:
            assert(false && "Invalid display color engine");
    }

    ds->frame_vsg->open();
    ds->vsg->generate_frame(scanbuf);
    ds->frame_vsg->close();

    SDL_FRect ii_frame_src;
    ii_frame_src = content_rec_vsg2[ds->video_scanner_type][(ds->new_video & 0x80) ? 1 : 0];
    ii_frame_src.x += (float)ds->hpos-ds->hsize;
    ii_frame_src.y += (float)ds->vpos-ds->vsize;
    ii_frame_src.w += (float)ds->hsize*2;
    ii_frame_src.h += (float)ds->vsize*2;

    ds->video_system->render_frame(ds->frame_vsg->get_texture(), &ii_frame_src, nullptr);

    return true;
}

/**
 * This is effectively a "redraw the entire screen each frame" method now.
 */

bool update_display_apple2(display_state_t *ds) {
    for (int i = 0; i < 17030; i++) {
        ds->video_scanner->video_cycle();
    }
    update_display_apple2_cycle(ds);
    return true;
}

void set_display_mode(display_state_t *ds, display_mode_t mode) {
    ds->display_mode = mode;
}

void set_split_mode(display_state_t *ds, display_split_mode_t mode) {
    ds->display_split_mode = mode;
}

void set_graphics_mode(display_state_t *ds, display_graphics_mode_t mode) {
    ds->display_graphics_mode = mode;
}


uint8_t txt_bus_read_C050(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set graphics mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Graphics Mode\n");
    set_display_mode(ds, GRAPHICS_MODE);
    /* if (!ds->framebased) */ ds->video_scanner->set_graf();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C050(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C050(context, address);
}


uint8_t txt_bus_read_C051(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
// set text mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Text Mode\n");
    set_display_mode(ds, TEXT_MODE);
    /* if (!ds->framebased) */ ds->video_scanner->set_text();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C051(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C051(context, address);
}


uint8_t txt_bus_read_C052(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set full screen
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Full Screen\n");
    set_split_mode(ds, FULL_SCREEN);
    /* if (!ds->framebased) */ ds->video_scanner->set_full();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C052(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C052(context, address);
}

uint8_t txt_bus_read_C053(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set split screen
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Split Screen\n");
    set_split_mode(ds, SPLIT_SCREEN);
    /* if (!ds->framebased) */ ds->video_scanner->set_mixed();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C053(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C053(context, address);
}

void reset_page2(display_state_t *ds) {
    // switch to page 1
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to page 1\n");
    ds->display_page_num = DISPLAY_PAGE_1;
    /* if (!ds->framebased) */ ds->video_scanner->set_page_1();
}

uint8_t txt_bus_read_C054(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 1
    reset_page2(ds);
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C054(void *context, uint32_t address, uint8_t value) {
    reset_page2((display_state_t *)context);
}

uint8_t txt_bus_read_C055(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 2
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to page 2\n");
    ds->display_page_num = DISPLAY_PAGE_2;
    /* if (!ds->framebased) */ ds->video_scanner->set_page_2();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C055(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C055(context, address);
}

// set lo-res (graphics) mode
void set_lores(display_state_t *ds) {
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set LoRes Mode\n");
    set_graphics_mode(ds, LORES_MODE);
    /* if (!ds->framebased) */ ds->video_scanner->set_lores();
}

uint8_t txt_bus_read_C056(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    set_lores(ds);
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C056(void *context, uint32_t address, uint8_t value) {
    set_lores((display_state_t *)context);
}

uint8_t txt_bus_read_C057(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set hi-res (graphics) mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set HiRes Mode\n");
    set_graphics_mode(ds, HIRES_MODE);
    /* if (!ds->framebased) */ ds->video_scanner->set_hires();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C057(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C057(context, address);
}

void ds_bus_write_C00X(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    switch (address) {
        case 0xC000:
            ds->video_scanner->set_80store(false);
            return;
        case 0xC001:
            ds->video_scanner->set_80store(true);
            return;
        case 0xC00C:
            ds->f_80col = false;
            break;
        case 0xC00D:
            ds->f_80col = true;
            break;
        default:
            assert(false && "ds_bus_write_C00X: Unhandled C00X write");
    }
    /* if (!ds->framebased) */ ds->video_scanner->set_80col_f(ds->f_80col);
    //update_line_mode(ds);
    //force_display_update(ds);
}

/**
 * display_state_t Class Implementation
 */
display_state_t::display_state_t() {

    display_mode = TEXT_MODE;
    display_split_mode = FULL_SCREEN;
    display_graphics_mode = LORES_MODE;
    display_page_num = DISPLAY_PAGE_1;

    flash_state = false;
    flash_counter = 0;
}

display_state_t::~display_state_t() {
    delete vsg;
    delete video_scanner;
    delete char_rom;
}

bool handle_display_event(display_state_t *ds, const SDL_Event &event) {
    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;
   
    if ( (mod & (SDL_KMOD_ALT | SDL_KMOD_SHIFT)) && (key == SDLK_KP_PLUS || key == SDLK_KP_MINUS)) {
        printf("key: %x, mod: %x\n", key, mod);
        if (mod & SDL_KMOD_ALT) { // ALT == hue (windows key on my mac)
            config.videoHue += ((key == SDLK_KP_PLUS) ? 0.025f : -0.025f);
            if (config.videoHue < -0.3f) config.videoHue = -0.3f;
            if (config.videoHue > 0.3f) config.videoHue = 0.3f;

        } else if (mod & SDL_KMOD_SHIFT) { // WINDOWS == brightness
            config.videoSaturation += ((key == SDLK_KP_PLUS) ? 0.1f : -0.1f);
            if (config.videoSaturation < 0.0f) config.videoSaturation = 0.0f;
            if (config.videoSaturation > 1.0f) config.videoSaturation = 1.0f;
        }
        init_hgr_LUT();
        static char msgbuf[256];
        snprintf(msgbuf, sizeof(msgbuf), "Hue set to: %f, Saturation to: %f\n", config.videoHue, config.videoSaturation);
        ds->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msgbuf));
        return true;
    }
    // TODO: get rid of these for production
    if (key == SDLK_F7) {
        if (mod & SDL_KMOD_CTRL) {
            // dump hires image page 1
            display_dump_hires_page(ds->mmu, 1);
            return true;
        }
        if (mod & SDL_KMOD_SHIFT) {
            // dump hires image page 2
            display_dump_hires_page(ds->mmu, 2);
            return true;
        }
    }
    if (key == SDLK_F8) {
        ds->vsg->setDumpNextFrame(true);
        return true;
    }
    return false;
}

void display_write_switches(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    switch (address) {
        case 0xC00E:
            ds->f_altcharset = false;
            break;
        case 0xC00F:
            ds->f_altcharset = true;
            break;
    }
    ds->video_scanner->set_altchrset_f(ds->f_altcharset);
}

/**
 * IIGS Video Control Registers
 */
uint8_t display_read_C029(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->new_video;
}

void set_new_video(display_state_t *ds, uint8_t value) {
    ds->new_video = value;
    if (ds->new_video & 0x80) {
        ds->video_scanner->set_shr();
        /* // TODO:ds->a2_display->set_shr(true); */
    } else {
        ds->video_scanner->reset_shr();
        /* // TODO: ds->a2_display->reset_shr(); */
    }
    if (ds->new_video & 0x20) {
        ds->vsg->set_dhgr_mono_mode(true);
    } else {
        ds->vsg->set_dhgr_mono_mode(false);
    }
}

void display_write_C029(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    set_new_video(ds, value);
}

/**
 * IIgs MONOCOLOR register
 */

/* There is no read C021, it's floating in GS */

void display_write_C021(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    if (value & 0x80) { 
        // enable mono
        ds->vsg->set_mono_mode(true);
    } else {
        ds->vsg->set_mono_mode(false);
    }
    
}

/**
 * IIGS Text Control Registers
 */

uint8_t display_read_C022(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->text_color;
}

void set_tbcolor(display_state_t *ds, uint8_t value) {
    ds->text_color = value;
    ds->video_scanner->set_text_bg(value & 0x0F);
    ds->video_scanner->set_text_fg(value >> 4);
    /* ds->a2_display->set_text_fg(value >> 4);
    ds->a2_display->set_text_bg(value & 0x0F); */
}

void display_write_C022(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    set_tbcolor(ds, value);
/*     ds->text_color = value;
    ds->video_scanner->set_text_bg(value & 0x0F);
    ds->video_scanner->set_text_fg(value >> 4);
    ds->a2_display->set_text_fg(value >> 4);
    ds->a2_display->set_text_bg(value & 0x0F); */
    /* // TODO: also set in AppleII_Display */
}

void set_bordercolor(display_state_t *ds, uint8_t value) {
    ds->border_color = value;
    ds->video_scanner->set_border_color(value);
    /* ds->a2_display->set_border_color(value); */
}

/*
 * CLOCKCTL - $C034 - 
 * this register is split between realtime clock and border color
 */

uint8_t display_read_C034(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->border_color;
}

void display_write_C034(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    set_bordercolor(ds, value & 0x0F);
/*     ds->border_color = value & 0x0F;
    ds->video_scanner->set_border_color(value);
    ds->a2_display->set_border_color(value); */
}

/**
 * IIe
 */

 // TODO: in a GS, should not mix in the floating bus with keyboard state. It should just be 0.
uint8_t display_read_C01AF(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = 0x00;

    switch (address) {
        case 0xC01A: // TEXT
            fl =  (ds->display_mode == TEXT_MODE) ? 0x80 : 0x00;
            break;
        case 0xC01B: // MIXED
            fl =  (ds->display_split_mode == SPLIT_SCREEN) ? 0x80 : 0x00;
            break;
        case 0xC01C: // PAGE2
            fl =  (ds->display_page_num == DISPLAY_PAGE_2) ? 0x80 : 0x00;
            break;
        case 0xC01D:  // HIRES
            fl =  (ds->display_graphics_mode == HIRES_MODE) ? 0x80 : 0x00;
            break;
        case 0xC01E: // ALTCHARSET
            fl =  (ds->f_altcharset) ? 0x80 : 0x00;
            break;
        case 0xC01F: // 80COL
            fl =  (ds->f_80col) ? 0x80 : 0x00;
            break;
        default:
            assert(false && "display_read_C01AF: Unhandled C01A-F read");
    }
    // TODO: fix this mess with the new bus concept.  
    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    //uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : ds->mmu->floating_bus_read()) & 0x7F;
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0x00) & 0x7F; // cover case of IIgs returning zero instead of floating here. II+/IIe will always have a keymsg.
    return kbv | fl;
}

uint8_t display_read_C05EF(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    ds->video_scanner->set_dblres_f(!ds->f_double_graphics);
    return ds->mmu->floating_bus_read();
}

void display_write_C05EF(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    ds->video_scanner->set_dblres_f(!ds->f_double_graphics);
}

/* VBL, Mouse, 1 sec, 1/4 sec Interrupt Handling Section - IIgs specific registers */

void display_write_c023(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    
    constexpr uint8_t VGCINT_MASK = 0b0000'0111;
    ds->f_VGCINT = (ds->f_VGCINT & ~VGCINT_MASK) | (value & VGCINT_MASK);
    //ds->f_VGCINT = (ds->f_VGCINT & 0b1111'1000) | (value & 0b0000'0111);

    update_vgc_interrupt(ds, false);
}

uint8_t display_read_c023(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->f_VGCINT;
}

void display_write_c041(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    value &= 0x1F; // bits 7-5 "must be 0"
    ds->f_INTEN = value;

    update_megaii_interrupt(ds, false);
}

uint8_t display_read_C041(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->f_INTEN;
}

/* C047 - CLRVBLINT - Clear VBL interrupt */
// Write of any value  (example is STZ CLRVBLINT) to clear these interrupts.
uint8_t display_read_c047(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_vblint_asserted = false;
    ds->f_quartersec_asserted = false;

    update_megaii_interrupt(ds, true);
    return ds->mmu->floating_bus_read(); // returns floating bus read per irqtest.s (Arekkusu)
}

void display_write_c047(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_vblint_asserted = false;
    ds->f_quartersec_asserted = false;

    update_megaii_interrupt(ds, true);
}

/*
 * C046 - INTFLAG 
 * the selftest / reset code checks bit 7, if 1, it jumps into selftest
  the mouse related bits in here are not used in IIgs.
 * 7 = 1 if mouse button currently down 
 * 6 = 1 if mouse was down on last read
 * 5 = AN3 returned in bit 5.
*/

uint8_t display_read_C046(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // update an3 value in the register..
    ds->f_an3_status = ds->f_double_graphics ? true : false;
    
    // this is identical to CPU irq_asserted.
    ds->f_system_irq_asserted = ds->irq_control->any_irq_asserted();
    return ds->f_INTFLAG;
}

/* End VBL Interrupt Handling Section */

/* The Apple IIgs Firmware Reference states that LANGSEL bit 3 is "0 if primary lang set selected", but this appears to be incorrect.
    Bit 3 is set to 1 by BRAM restore during power-on (or booting GS/OS, or entering the Control Panel) and hardware testing shows that 
    the language in bits 5-7 is ignored when bit 3 is 0.    */
void set_langsel(display_state_t *ds, uint8_t value) {
    ds->f_langsel = value & 0b1111'1000;
    
    // set language for display. Only values 0-7 are valid.
    ds->vsg->set_char_set((ds->f_langsel & 0xE0) >> 5); // set LS scanner.
    
    // TODO: set video mode timing ntsc vs pal.
    // TODO: implement LANGUAGE switch (if 0, use lang 0. Otherwise use whatever lang selected.)
}

/* C02B - LANGSEL - IIgs specific */
void display_write_C02B(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    set_langsel(ds, value & 0b1111'1000);
}

uint8_t display_read_C02B(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->f_langsel;
}

/* VBL Read */

uint8_t display_read_vbl(void *context, uint32_t address) {
    // This is enough to get basic VBL working. Total Replay boots anyway.
    display_state_t *ds = (display_state_t *)context;

    uint8_t fl = (ds->video_scanner->is_vbl()) ? 0x00 : 0x80;
    if (ds->video_scanner_type == Scanner_AppleIIgs) { // inverted sense on IIgs.
        fl ^= 0x80;
    }
    // TODO: fix this mess with the new bus concept.  
    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    //uint8_t kbv = (keymsg ? keymsg->mk->last_key_val :  ds->mmu->floating_bus_read()) & 0x7F;
    // same fix as above for IIgs returning zero instead of floating here.
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val :  0x00) & 0x7F;
    return kbv | fl;
}

void update_vgc_interrupt(display_state_t *ds, bool assert_now) {
    uint8_t vgc_asserted = 0;
    if (
        (ds->f_scanline_enable && ds->f_scanline_asserted)  || 
        (ds->f_onesec_enable && ds->f_onesec_asserted) /*  || 
        (ds->f_quartersec_enable && ds->f_quartersec_asserted) ||
        (ds->f_vbl_enable && ds->f_vblint_asserted) */
    ) vgc_asserted = 1;
    ds->f_vgcint_asserted = vgc_asserted;
    if (assert_now && vgc_asserted) {
        ds->irq_control->assert_irq(IRQ_ID_VGC);
    } else {
        ds->irq_control->deassert_irq(IRQ_ID_VGC);
    }
}

void update_megaii_interrupt(display_state_t *ds, bool assert_now) {
    uint8_t megaii_asserted = 0;
    if (
        (ds->f_quartersec_enable && ds->f_quartersec_asserted) ||
        (ds->f_vbl_enable && ds->f_vblint_asserted)
    ) megaii_asserted = 1;
    //ds->f_system_irq_asserted = megaii_asserted;
    if (assert_now && megaii_asserted) {
        ds->irq_control->assert_irq(IRQ_ID_MEGAII);
    } else {
        ds->irq_control->deassert_irq(IRQ_ID_MEGAII);
    }
}

/* 
Calculate the video counters (as they would exist in the real video scanner), compose them as they are 
in the IIgs, and return whichever one was asked-for.
*/
uint8_t display_read_C02EF(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;

    ds->f_scanline_asserted = false;
    update_vgc_interrupt(ds, true); // should clear flag and deassert IRQ

    uint16_t vcounter = ds->video_scanner->get_vcounter() & 0x1FF;
    uint16_t hcounter = ds->video_scanner->get_hcounter() & 0x7F;
    uint8_t c02e = (vcounter >> 1);
    uint8_t c02f = ((vcounter & 0x1) << 7) | hcounter;
    if (address & 0x1) return c02f;
    else return c02e;
}

void display_update_video_scanner(display_state_t *ds) {
    if (ds->clock->get_clock_mode() == CLOCK_FREE_RUN) {
        ds->framebased = true;
        ds->clock->set_video_scanner(nullptr);
    } else {
        ds->framebased = false;
        ds->clock->set_video_scanner(ds->video_scanner);
    }
}

/* accepts a VideoScannerEvent and updates the display interrupts accordingly */
void scanner_iigs_handler(void *context, VideoScannerEvent event) {
    display_state_t *ds = (display_state_t *)context;

    switch (event) {
        case VS_EVENT_SCB_INTERRUPT:
            ds->f_scanline_asserted = true;
            update_vgc_interrupt(ds, true);
            break;
        case VS_EVENT_VBL:
            ds->f_vblint_asserted = true;
            update_megaii_interrupt(ds, true);
            
            break;
        case VS_EVENT_QTR:
            // every 16 frames, also assert 0.25 sec
            if (++ds->quartersec_counter == 16) {
                ds->f_quartersec_asserted = true; // only assert if enabled.
                ds->quartersec_counter = 0; // kinda need to reset the counter bro.
            }
            update_megaii_interrupt(ds, true);
            break;
        default:
            assert(false && "Unhandled VideoScannerEvent in scanner_iigs_handler");
            break;
    }
}

/*
 in a real IIgs, the one second trigger comes out of the realtime clock, on UG3.1 to UH2.57.
 This is probably a signal that ticks high for a bit when the RTC ticks over to a new second,
 so the IRQ in VGC for 1 second is edge triggered low to hi, which is why there is no "reset"
 for this IRQ source.

 This routine could check for skew and adjust the schedule accordingly, but that is not implemented
 currently.
*/
void rtc_pram_1sec_interrupt(uint64_t instanceID, void *context) {
    display_state_t *ds = (display_state_t *)context;
    // throw interrupt
    ds->f_onesec_asserted = true;
    update_vgc_interrupt(ds, true);
    // reschedule ourselves for 1 second.
    uint64_t trigger_cycle = ds->clock->get_c14m() + ds->clock->get_c14m_per_second() /* 14318180 */;
    ds->computer->event_timer->scheduleEvent(trigger_cycle, rtc_pram_1sec_interrupt, instanceID, ds);
}

void display_write_c032(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    if ((value & 0x40) == 0) ds->f_onesec_asserted = false;
    if ((value & 0x20) == 0) ds->f_scanline_asserted = false;
    update_vgc_interrupt(ds, true);
}

const char *scanner_to_string(video_scanner_t scanner_type) {
    switch (scanner_type) {
        case Scanner_AppleII: return "Apple II";
        case Scanner_AppleIIe: return "Apple IIe";
        case Scanner_AppleIIePAL: return "Apple IIe PAL";
        case Scanner_AppleIIgs: return "Apple IIgs";
    }
    return "Unknown";
}

DebugFormatter *display_debug(display_state_t *ds) {
    DebugFormatter *df = new DebugFormatter();
    df->addLine("Display State:");
    //df->addLine("  Framebased: %d", ds->framebased);
    df->addLine("  Video Scanner: (%d) %s", ds->video_scanner_type, scanner_to_string(ds->video_scanner_type));
    df->addLine("  Mode: %s %s %s %s %s %s %s", 
        ds->display_graphics_mode == LORES_MODE ? "LORES" : "HIRES",
        ds->display_mode == TEXT_MODE ? "TEXT" : "GRAPHICS",
        ds->display_split_mode == SPLIT_SCREEN ? "SPLIT" : "FULL",
        ds->display_page_num == DISPLAY_PAGE_1 ? "PAGE1" : "PAGE2",
        ds->f_80col ? "80COL" : "40COL",
        ds->f_double_graphics ? "DBL" : "SNG",
        ds->f_altcharset ? "ALTCHARSET" : "NORMAL");
    df->addLine("  SHR: %s", ds->new_video & 0x80 ? "ON" : "OFF");

    uint32_t hcount = ds->video_scanner->get_hcount();
    uint32_t vcount = ds->video_scanner->get_vcount();
    bool is_hbl = ds->video_scanner->is_hbl();
    bool is_vbl = ds->video_scanner->is_vbl();
    df->addLine(" Beam position: H %3d V %3d Is_HBL %d Is_VBL %d", hcount, vcount, is_hbl, is_vbl);
    df->addLine(" HCOUNTER %02X VCOUNTER %03X", ds->video_scanner->get_hcounter(), ds->video_scanner->get_vcounter());
    df->addLine(" INTEN C041: %02X VGCINT C023: %02X INTFLAG C046: %02X", ds->f_INTEN, ds->f_VGCINT, ds->f_INTFLAG);
    df->addLine("    sl en: %d as: %d - 1sec en: %d as: %d", ds->f_scanline_enable, ds->f_scanline_asserted, ds->f_onesec_enable, ds->f_onesec_asserted);
    return df;
}

void init_mb_device_display_common(computer_t *computer, SlotType_t slot, bool cycleaccurate) {
    cpu_state *cpu = computer->cpu;
    
    // alloc and init display state
    display_state_t *ds = new display_state_t;
    video_system_t *vs = computer->video_system;

    ds->framebased = false;
    ds->mbus = computer->mbus;
    ds->irq_control = computer->irq_control;
    ds->video_system = vs;
    ds->event_queue = computer->event_queue;
    ds->computer = computer;
    ds->clock = computer->clock;
    MMU_II *mmu = computer->mmu;
    ds->mmu = mmu;

    // Grab appropriate Character ROM and load it.
    // TODO: this is a hack, platforms selects ROMs directory and we should reference that.
    CharRom *charrom = nullptr;
    // Construct char rom path
    std::string charrom_path = "roms/";
    charrom_path.append(computer->platform->rom_dir);
    charrom_path.append("/char.rom");
    charrom = new CharRom(charrom_path.c_str());

    /* switch (computer->platform->id) {
        case PLATFORM_APPLE_IIE:
            charrom = new CharRom("roms/apple2e/char.rom");
            break;
        case PLATFORM_APPLE_IIE_ENHANCED:
        case PLATFORM_APPLE_IIE_65816:
            charrom = new CharRom("roms/apple2e_enh/char.rom");
            break;
        case PLATFORM_APPLE_IIGS:
            charrom = new CharRom("roms/apple2gs/char.rom");
            break;
        case PLATFORM_APPLE_II_PLUS:
            charrom = new CharRom("roms/apple2_plus/char.rom");
            break;
        case PLATFORM_APPLE_II:
            charrom = new CharRom("roms/apple2/char.rom");
            break;
        default:
            system_failure("Unsupported platform in display engine init");
            break;
    } */
    
    // create VideoScanner stuff if desired
    ds->video_scanner_type = computer->get_video_scanner();
    switch (ds->video_scanner_type) {
        case Scanner_AppleIIgs:
            ds->video_scanner = new VideoScannerIIgs(mmu);
            ds->video_scanner->initialize();
            ds->video_scanner->set_irq_handler({scanner_iigs_handler, ds});
            break;
        case Scanner_AppleIIePAL:
            ds->video_scanner = new VideoScannerIIePAL(mmu);
            ds->video_scanner->initialize();
            break;
        case Scanner_AppleIIe:
            ds->video_scanner = new VideoScannerIIe(mmu);
            ds->video_scanner->initialize();
            break;
        case Scanner_AppleII:
            ds->video_scanner = new VideoScannerII(mmu);
            ds->video_scanner->initialize();
            break;
        default:
            system_failure("Unsupported VideoScanner type in display engine init");
            break;
    }

    // Initialize the VideoScanGenerators with the CharRom, and frame.
    ds->frame_vsg = new(std::align_val_t(64)) FrameVSG(910, 263, vs->renderer, PIXEL_FORMAT);
    ds->vsgr = new VideoScanGenerator_RGB(charrom, true, ds->frame_vsg);
    ds->vsgc = new VideoScanGenerator_Comp(charrom, false, ds->frame_vsg);
    ds->vsgc->set_render(&ds->mon_ntsc);

    switch (computer->platform->id) {
        case PLATFORM_APPLE_IIE_65816:
        case PLATFORM_APPLE_IIGS:
            ds->vsg = ds->vsgr;
            break;
        default:
            ds->vsg = ds->vsgc;
            break;
    }

    ds->char_rom = charrom;
 
    // LINEAR gets us appropriately blurred pixels, NEAREST gets us sharp pixels, PIXELART is sharper pixels that are more accurate
    // linear/pixelart are set in vs->render_frame.
    SDL_SetTextureBlendMode(ds->frame_vsg->get_texture(), SDL_BLENDMODE_NONE);

    // set in CPU so we can reference later
    computer->set_module_state(MODULE_DISPLAY, ds);
    
    mmu->set_C0XX_read_handler(0xC050, { txt_bus_read_C050, ds });
    mmu->set_C0XX_write_handler(0xC050, { txt_bus_write_C050, ds });
    mmu->set_C0XX_read_handler(0xC051, { txt_bus_read_C051, ds });
    mmu->set_C0XX_read_handler(0xC052, { txt_bus_read_C052, ds });
    mmu->set_C0XX_write_handler(0xC051, { txt_bus_write_C051, ds });
    mmu->set_C0XX_write_handler(0xC052, { txt_bus_write_C052, ds });
    mmu->set_C0XX_read_handler(0xC053, { txt_bus_read_C053, ds });
    mmu->set_C0XX_write_handler(0xC053, { txt_bus_write_C053, ds });
    mmu->set_C0XX_read_handler(0xC054, { txt_bus_read_C054, ds });
    mmu->set_C0XX_write_handler(0xC054, { txt_bus_write_C054, ds });
    mmu->set_C0XX_read_handler(0xC055, { txt_bus_read_C055, ds });
    mmu->set_C0XX_write_handler(0xC055, { txt_bus_write_C055, ds });
    mmu->set_C0XX_read_handler(0xC056, { txt_bus_read_C056, ds });
    mmu->set_C0XX_write_handler(0xC056, { txt_bus_write_C056, ds });
    mmu->set_C0XX_read_handler(0xC057, { txt_bus_read_C057, ds });
    mmu->set_C0XX_write_handler(0xC057, { txt_bus_write_C057, ds });

    display_update_video_scanner(ds);

    computer->sys_event->registerHandler(SDL_EVENT_KEY_UP, [ds](const SDL_Event &event) {
        return handle_display_event(ds, event);
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [ds](const SDL_Event &event) {
        int key = event.key.key;
        if ((key == SDLK_F7) || (key == SDLK_F8) || (key == SDLK_KP_PLUS) || (key == SDLK_KP_MINUS)) {
            return true; // eat the keydown
        }
        return false;
    });

    computer->register_shutdown_handler([ds]() {
        delete ds;
        return true;
    });

    computer->register_reset_handler([ds](bool cold_start) {
        if (ds->computer->platform->id == PLATFORM_APPLE_IIGS) {
            display_write_c041(ds, 0xC041, 0x00);
            // TODO: this is the cleanest way to do it for now, but it feels a little hacky, as if
            // reset handler in mmu and here should each be responsible for clearing their own bits.
            // NEWVIDEO
            ds->mmu->write(0xC029, ds->new_video&0x1);
            //set_new_video(ds, 0x00); // C029
            //LANGSEL: no change
            set_tbcolor(ds, 0x0F0); // C022
            set_bordercolor(ds, 0x00); // C034
            // C023 - VGCINT to 0
            display_write_c023(ds, 0xC023, 0x00);
            // C021 MONOCOLOR
            display_write_C021(ds, 0xC021, 0x00);
        }
        if (ds->computer->platform->id >= PLATFORM_APPLE_IIE) { // iie and GS share these..
            // TEXT/GRAPHICS: no change.
            // MIXED: no change.

            // 80col: force to 0
            ds->f_80col = false;

            ds->f_altcharset = false;
            ds->video_scanner->reset_80col();
            
            // ALTCHARSET: force to 0
            ds->video_scanner->reset_altchrset();
            
            // set page2 off. make sure also in mmu
            reset_page2(ds);
        }
        if (ds->computer->platform->id < PLATFORM_APPLE_IIGS) { // iie and below only
            // C05E: set ANC3 to 7M video mode. "set dblres" is opposite sense of ANC3 OFF.
            ds->f_double_graphics = true;
            ds->video_scanner->set_dblres();
            
            // LORES: switch reset
            set_lores(ds);  // TODO: is this also on a II+?   
        }
        // What about the II/II+????
        //update_line_mode(ds);
        return true;
    });

    if (computer->platform->id == PLATFORM_APPLE_IIE || computer->platform->id == PLATFORM_APPLE_IIE_ENHANCED
    || computer->platform->id == PLATFORM_APPLE_IIE_65816 || computer->platform->id == PLATFORM_APPLE_IIGS) {
        ds->f_altcharset = false;

        mmu->set_C0XX_write_handler(0xC000, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC001, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC00C, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC00D, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC00E, { display_write_switches, ds });
        mmu->set_C0XX_write_handler(0xC00F, { display_write_switches, ds });
        
        mmu->set_C0XX_read_handler(0xC01A, { display_read_C01AF, ds });
        mmu->set_C0XX_read_handler(0xC01B, { display_read_C01AF, ds });
        mmu->set_C0XX_read_handler(0xC01C, { display_read_C01AF, ds });
        mmu->set_C0XX_read_handler(0xC01D, { display_read_C01AF, ds });
        mmu->set_C0XX_read_handler(0xC01E, { display_read_C01AF, ds });
        mmu->set_C0XX_read_handler(0xC01F, { display_read_C01AF, ds });
        
        mmu->set_C0XX_read_handler(0xC05E, { display_read_C05EF, ds });
        mmu->set_C0XX_write_handler(0xC05E, { display_write_C05EF, ds });
        mmu->set_C0XX_read_handler(0xC05F, { display_read_C05EF, ds });
        mmu->set_C0XX_write_handler(0xC05F, { display_write_C05EF, ds });
        mmu->set_C0XX_read_handler(0xC019, { display_read_vbl, ds });

    }
    if (computer->platform->id == PLATFORM_APPLE_IIGS) {
        mmu->set_C0XX_write_handler(0xC023, { display_write_c023, ds });
        mmu->set_C0XX_read_handler(0xC023, { display_read_c023, ds });
        mmu->set_C0XX_write_handler(0xC041, { display_write_c041, ds });
        mmu->set_C0XX_read_handler(0xC041, { display_read_C041, ds });
        mmu->set_C0XX_read_handler(0xC046, { display_read_C046, ds });
        mmu->set_C0XX_write_handler(0xC047, { display_write_c047, ds });
        mmu->set_C0XX_read_handler(0xC047, { display_read_c047, ds });
        mmu->set_C0XX_write_handler(0xC032, { display_write_c032, ds });
        mmu->set_C0XX_write_handler(0xC02B, { display_write_C02B, ds });
        mmu->set_C0XX_read_handler(0xC02B, { display_read_C02B, ds });
    }

    if (computer->platform->id == PLATFORM_APPLE_IIE_65816 || computer->platform->id == PLATFORM_APPLE_IIGS) {
        mmu->set_C0XX_read_handler(0xC02E, { display_read_C02EF, ds });
        mmu->set_C0XX_read_handler(0xC02F, { display_read_C02EF, ds });
        //no display_read_C021;  actually floating bus all the time.
        mmu->set_C0XX_write_handler(0xC021, { display_write_C021, ds });

        mmu->set_C0XX_read_handler(0xC029, { display_read_C029, ds });
        mmu->set_C0XX_write_handler(0xC029, { display_write_C029, ds });
        mmu->set_C0XX_read_handler(0xC022, { display_read_C022, ds });
        mmu->set_C0XX_write_handler(0xC022, { display_write_C022, ds });
        mmu->set_C0XX_read_handler(0xC034, { display_read_C034, ds });
        mmu->set_C0XX_write_handler(0xC034, { display_write_C034, ds });
        ds->vsg->set_display_shift(false); // no shift in Apple IIgs mode.
        ds->mon_mono.set_shift_enabled(false);
        ds->mon_ntsc.set_shift_enabled(false);
        //ds->mon_rgb.set_shift_enabled(false);
        
        // Set default video scanner colors for Apple IIgs. (F, 6, 6)
        set_tbcolor(ds, 0xF0);
        set_bordercolor(ds, 0x00);
    }

    vs->register_frame_processor(0, [ds](bool force_full_frame) -> bool {
        bool ret;
        if (ds->framebased || force_full_frame) {
            ret = update_display_apple2(ds);
        } else {
            ret = update_display_apple2_cycle(ds);
        }
        return ret;
    });

    if (ds->video_scanner_type == Scanner_AppleIIgs) {
        // For generating the 1sec interrupt, try to sync to real time as close to a 1 second increment as possible,
        // based on module startup time.
        SDL_Time time;
        SDL_GetCurrentTime(&time); // get current time in nanoseconds.
        uint64_t remain = 1000000000 - (time % 1000000000);
        // how many nanoseconds is a 14M
        uint64_t ns_14m = 1000000000 / ds->clock->get_c14m_per_second() /* 14318180 */;
        // calculate number of 14M ticks (14318180hz) are in remain nanoseconds
        uint64_t ticks_14m = remain / ns_14m;
        // set the 14M timer to the number of ticks
        computer->event_timer->scheduleEvent(ticks_14m, rtc_pram_1sec_interrupt, 0xFF112200, ds);
    }
    computer->register_debug_display_handler(
        "display",
        DH_DISPLAY, // unique ID for this, need to have in a header.
        [ds]() -> DebugFormatter * {
            return display_debug(ds);
        }
    );
}

void init_mb_device_display(computer_t *computer, SlotType_t slot) {
    init_mb_device_display_common(computer, slot, true);
}

/* void init_mb_device_display_frameonly(computer_t *computer, SlotType_t slot) {
    init_mb_device_display_common(computer, slot, false);
} */

void display_dump_file(MMU_II *mmu, const char *filename, uint16_t base_addr, uint16_t sizer) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return;
    }
    // Write 8192 bytes (0x2000) from memory starting at base_addr
    for (uint16_t offset = 0; offset < sizer; offset++) {
        uint8_t byte = mmu->read(base_addr + offset);
        fwrite(&byte, 1, 1, fp);
    }
    fclose(fp);
}

void display_dump_hires_page(MMU_II *mmu, int page) {
    uint16_t base_addr = (page == 1) ? 0x2000 : 0x4000;
    display_dump_file(mmu, "dump.hgr", base_addr, 0x2000);
    fprintf(stdout, "Dumped HGR page %d to dump.hgr\n", page);
}

void display_dump_text_page(MMU_II *mmu, int page) {
    uint16_t base_addr = (page == 1) ? 0x0400 : 0x0800;
    display_dump_file(mmu, "dump.txt", base_addr, 0x0400);
    fprintf(stdout, "Dumped TXT page %d to dump.txt\n", page);
}
