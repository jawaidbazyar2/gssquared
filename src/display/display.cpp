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

#include "cpu.hpp"
#include "gs2.hpp"
#include "debug.hpp"

#include "display.hpp"
#include "text_40x24.hpp"
#include "lores_40x48.hpp"
#include "hgr_280x192.hpp"
#include "platforms.hpp"
#include "event_poll.hpp"

#include "util/dialog.hpp"

#include "display/displayng.hpp"
#include "display/hgr.hpp"
#include "display/lgr.hpp"
#include "display/ntsc.hpp"

#include "videosystem.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "mbus/MessageBus.hpp"
#include "mbus/KeyboardMessage.hpp"

display_page_t display_pages[NUM_DISPLAY_PAGES] = {
    {
        0x0400,
        0x07FF,
        {   // text page 1 line addresses
            0x0400,
            0x0480,
            0x0500,
            0x0580,
            0x0600,
            0x0680,
            0x0700,
            0x0780,

            0x0428,
            0x04A8,
            0x0528,
            0x05A8,
            0x0628,
            0x06A8,
            0x0728,
            0x07A8,

            0x0450,
            0x04D0,
            0x0550,
            0x05D0,
            0x0650,
            0x06D0,
            0x0750,
            0x07D0,
        },
        0x2000,
        0x3FFF,
        { // HGR page 1 line addresses
            0x2000,
            0x2080,
            0x2100,
            0x2180,
            0x2200,
            0x2280,
            0x2300,
            0x2380,

            0x2028,
            0x20A8,
            0x2128,
            0x21A8,
            0x2228,
            0x22A8,
            0x2328,
            0x23A8,

            0x2050,
            0x20D0,
            0x2150,
            0x21D0,
            0x2250,
            0x22D0,
            0x2350,
            0x23D0,
        },
    },
    {
        0x0800,
        0x0BFF,
        {       // text page 2 line addresses
            0x0800,
            0x0880,
            0x0900,
            0x0980,
            0x0A00,
            0x0A80,
            0x0B00,
            0x0B80,

            0x0828,
            0x08A8,
            0x0928,
            0x09A8,
            0x0A28,
            0x0AA8,
            0x0B28,
            0x0BA8,

            0x0850,
            0x08D0,
            0x0950,
            0x09D0,
            0x0A50,
            0x0AD0,
            0x0B50,
            0x0BD0,
        },
        0x4000,
        0x5FFF,
        {       // HGR page 2 line addresses
            0x4000,
            0x4080,
            0x4100,
            0x4180,
            0x4200,
            0x4280,
            0x4300,
            0x4380,

            0x4028,
            0x40A8,
            0x4128,
            0x41A8,
            0x4228,
            0x42A8,
            0x4328,
            0x43A8,

            0x4050,
            0x40D0,
            0x4150,
            0x41D0,
            0x4250,
            0x42D0,
            0x4350,
            0x43D0,
        },
    },
};

// TODO: These should be set from an array of parameters.
void set_display_page(display_state_t *ds, display_page_number_t page) {
    ds->display_page_table = &display_pages[page];
    ds->display_page_num = page;
}

void set_display_page1(display_state_t *ds) {
    set_display_page(ds, DISPLAY_PAGE_1);
}

void set_display_page2(display_state_t *ds) {
    set_display_page(ds, DISPLAY_PAGE_2);
}

void init_display_font(rom_data *rd) {
    pre_calculate_font(rd);
}

#if 0
/**
 * This is effectively a "redraw the entire screen each frame" method now.
 * With an optimization only update dirty lines.
 */
bool update_display_apple2(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    // the backbuffer must be cleared each frame. The docs state this clearly
    // but I didn't know what the backbuffer was. Also, I assumed doing it once
    // at startup was enough. NOPE. (oh, it's buffer flipping).

    int updated = 0;
    for (int line = 0; line < 24; line++) {
        if (vs->force_full_frame_redraw || ds->dirty_line[line]) {
            switch (vs->display_color_engine) {
                case DM_ENGINE_NTSC:
                    render_line_ntsc(cpu, line);
                    break;
                case DM_ENGINE_RGB:
                    render_line_rgb(cpu, line);
                    break;
                default:
                    render_line_mono(cpu, line);
                    break;
            }
            ds->dirty_line[line] = 0;
            updated = 1;
        }
    }

    if (updated) { // only reload texture if we updated any lines.
        void* pixels;
        int pitch;
        if (!SDL_LockTexture(ds->screenTexture, NULL, &pixels, &pitch)) {
            fprintf(stderr, "Failed to lock texture: %s\n", SDL_GetError());
            return true;
        }
        memcpy(pixels, ds->buffer, BASE_WIDTH * BASE_HEIGHT * sizeof(RGBA_t)); // load all buffer into texture
        SDL_UnlockTexture(ds->screenTexture);
    }
    vs->force_full_frame_redraw = false;
    vs->render_frame(ds->screenTexture);
    return true;
}
#else

/**
 * This is effectively a "redraw the entire screen each frame" method now.
 * With an optimization only update dirty lines.
 */
bool update_display_apple2(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    ds->vbl_cycle_count = cpu->cycles;

    // first push flash state into AppleII_Display
    ds->a2_display->set_flash_state(ds->flash_state);

    // the backbuffer must be cleared each frame. The docs state this clearly
    // but I didn't know what the backbuffer was. Also, I assumed doing it once
    // at startup was enough. NOPE. (oh, it's buffer flipping).
    uint8_t *ram = ds->mmu->get_memory_base();
    uint8_t *text_page;
    uint8_t *hgr_page;
    uint8_t *alt_text_page;
    uint8_t *alt_hgr_page;
    if (ds->display_page_num == DISPLAY_PAGE_1) {
        text_page     = ram + 0x0400;
        alt_text_page = ram + 0x10400;
        hgr_page      = ram + 0x2000;
        alt_hgr_page  = ram + 0x12000;
    } else {
        text_page     = ram + 0x0800;
        alt_text_page = ram + 0x10800;
        hgr_page      = ram + 0x4000;
        alt_hgr_page  = ram + 0x14000;
    }

    int updated = 0;
    for (uint16_t line = 0; line < 24; line++) {
        if (vs->force_full_frame_redraw || ds->dirty_line[line]) {
            switch (ds->line_mode[line]) {
                case LM_TEXT_MODE:
                    ds->a2_display->generate_text40(text_page, ds->frame_bits, line);
                    break;
                case LM_LORES_MODE:
                    ds->a2_display->generate_lores40(text_page, ds->frame_bits, line);
                    break;
                case LM_HIRES_MODE:
                    ds->a2_display->generate_hires40(hgr_page, ds->frame_bits, line);
                    break;
                case LM_TEXT80_MODE:
                    ds->a2_display->generate_text80(text_page, alt_text_page, ds->frame_bits, line);
                    break;
                case LM_LORES80_MODE:
                    ds->a2_display->generate_lores80(text_page, alt_text_page, ds->frame_bits, line);
                    break;
                case LM_HIRES80_MODE:
                    ds->a2_display->generate_hires80(hgr_page, alt_hgr_page, ds->frame_bits, line);
                    break;
            }
            ds->dirty_line[line] = 0;
            updated = 1;
        }
    }

    if (updated) { // only reload texture if we updated any lines.
        RGBA_t mono_color_value = vs->get_mono_color();
        
        // do a switch on display engine later..
        switch (vs->display_color_engine) {
            case DM_ENGINE_NTSC:
                if (ds->display_mode == TEXT_MODE) {
                    ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, (RGBA_t){.a = 0xFF, .b = 0xFF, .g = 0xFF, .r = 0xFF});
                } else {
                    ds->mon_ntsc.render(ds->frame_bits, ds->frame_rgba, (RGBA_t){.a = 0xFF, .b = 0x00, .g = 0xFF, .r = 0x00}, 1);
                }
                break;
            case DM_ENGINE_RGB:
                ds->mon_rgb.render(ds->frame_bits, ds->frame_rgba, (RGBA_t){.a = 0xFF, .b = 0x00, .g = 0xFF, .r = 0x00}, 1);
                break;
            case DM_ENGINE_MONO:
                ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, mono_color_value);
                break;
            default:
                break; // never happens
        }

        void* pixels;
        int pitch;
        if (!SDL_LockTexture(ds->screenTexture, NULL, &pixels, &pitch)) {
            fprintf(stderr, "Failed to lock texture: %s\n", SDL_GetError());
            return true;
        }
        memcpy(pixels, ds->frame_rgba->data(), (BASE_WIDTH+20) * BASE_HEIGHT * sizeof(RGBA_t)); // load all buffer into texture
        SDL_UnlockTexture(ds->screenTexture);
    }
    vs->force_full_frame_redraw = false;
    vs->render_frame(ds->screenTexture, -7.0f);
    return true;
}

#endif

/* void update_display(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    annunciator_state_t * anc_d = (annunciator_state_t *)get_module_state(cpu, MODULE_ANNUNCIATOR);
    videx_data * videx_d = (videx_data *)get_slot_state_by_id(cpu, DEVICE_ID_VIDEX);

    // the backbuffer must be cleared each frame. The docs state this clearly
    // but I didn't know what the backbuffer was. Also, I assumed doing it once
    // at startup was enough. NOPE.
    ds->video_system->clear();

    if (videx_d && ds->display_mode == TEXT_MODE && anc_d && anc_d->annunciators[0] ) {
        update_display_videx(cpu, videx_d ); 
    } else {
        update_display_apple2(cpu);
    }
    // TODO: IIgs will need a hook here too - do same video update callback function.
} */

void force_display_update(display_state_t *ds) {
    for (int y = 0; y < 24; y++) {
        ds->dirty_line[y] = 1;
    }
}

void update_line_mode(display_state_t *ds) {

    line_mode_t top_mode;
    line_mode_t bottom_mode;

    if (ds->display_mode == TEXT_MODE) {
        if (ds->f_80col) {
            top_mode = LM_TEXT80_MODE;
        } else {
            top_mode = LM_TEXT_MODE;
        }
    } else {
        if (ds->display_graphics_mode == LORES_MODE) {
            if (!ds->f_double_graphics) {
                top_mode = LM_LORES80_MODE;
            } else {
                top_mode = LM_LORES_MODE;
            }
        } else {
            if (!ds->f_double_graphics) {
                top_mode = LM_HIRES80_MODE;
            } else {
                top_mode = LM_HIRES_MODE;
            }
        }
    }

    if (ds->display_split_mode == SPLIT_SCREEN) {
        if (ds->f_80col) {
            bottom_mode = LM_TEXT80_MODE;
        } else {
            bottom_mode = LM_TEXT_MODE;
        }
    } else {
        bottom_mode = top_mode;
    }

    for (int y = 0; y < 20; y++) {
        ds->line_mode[y] = top_mode;
    }
    for (int y = 20; y < 24; y++) {
        ds->line_mode[y] = bottom_mode;
    }
}

void set_display_mode(display_state_t *ds, display_mode_t mode) {

    ds->display_mode = mode;
    update_line_mode(ds);
}

void set_split_mode(display_state_t *ds, display_split_mode_t mode) {

    ds->display_split_mode = mode;
    update_line_mode(ds);
}

void set_graphics_mode(display_state_t *ds, display_graphics_mode_t mode) {

    ds->display_graphics_mode = mode;
    update_line_mode(ds);
}

// anything we lock we have to completely replace.

void render_line_ntsc(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;
    // this writes into texture - do not put border stuff here.

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lgrng_scanline(cpu, y);
    else if (mode == LM_HIRES_MODE) render_hgrng_scanline(cpu, y, (uint8_t *)pixels);
    else render_text_scanline_ng(cpu, y);

    RGBA_t mono_color_value = { 0xFF, 0xFF, 0xFF, 0xFF }; // override mono color to white when we're in color mode

    if (ds->display_mode == TEXT_MODE) {
        processAppleIIFrame_Mono(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8, mono_color_value);
    } else {
        processAppleIIFrame_LUT(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8);
    }

}

void render_line_rgb(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lores_scanline(cpu, y, pixels, pitch);
    else if (mode == LM_HIRES_MODE) render_hgr_scanline(cpu, y, pixels, pitch);
    else render_text_scanline(cpu, y, pixels, pitch);

}

void render_line_mono(cpu_state *cpu, int y) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    RGBA_t mono_color_value ;

    void* pixels = ds->buffer + (y * 8 * BASE_WIDTH * sizeof(RGBA_t));
    int pitch = BASE_WIDTH * sizeof(RGBA_t);

    line_mode_t mode = ds->line_mode[y];

    if (mode == LM_LORES_MODE) render_lgrng_scanline(cpu, y);
    else if (mode == LM_HIRES_MODE) render_hgrng_scanline(cpu, y, (uint8_t *)pixels);
    else render_text_scanline_ng(cpu, y);

    mono_color_value = vs->get_mono_color();

    processAppleIIFrame_Mono(frameBuffer + (y * 8 * BASE_WIDTH), (RGBA_t *)pixels, y * 8, (y + 1) * 8, mono_color_value);
}

uint8_t txt_bus_read_C050(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set graphics mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Graphics Mode\n");
    set_display_mode(ds, GRAPHICS_MODE);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C050(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C050(context, address);
}


uint8_t txt_bus_read_C051(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
// set text mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Text Mode\n");
    set_display_mode(ds, TEXT_MODE);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C051(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C051(context, address);
}


uint8_t txt_bus_read_C052(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set full screen
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Full Screen\n");
    set_split_mode(ds, FULL_SCREEN);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C052(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C052(context, address);
}


uint8_t txt_bus_read_C053(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set split screen
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Split Screen\n");
    set_split_mode(ds, SPLIT_SCREEN);
    ds->video_system->set_full_frame_redraw();
    return 0;
}
void txt_bus_write_C053(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C053(context, address);
}


uint8_t txt_bus_read_C054(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 1
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to screen 1\n");
    set_display_page1(ds);
    ds->video_system->set_full_frame_redraw();
    return 0;
}
void txt_bus_write_C054(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C054(context, address);
}


uint8_t txt_bus_read_C055(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 2
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to screen 2\n");
    set_display_page2(ds);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C055(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C055(context, address);
}


uint8_t txt_bus_read_C056(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set lo-res (graphics) mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Lo-Res Mode\n");
    set_graphics_mode(ds, LORES_MODE);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C056(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C056(context, address);
}

uint8_t txt_bus_read_C057(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set hi-res (graphics) mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Hi-Res Mode\n");
    set_graphics_mode(ds, HIRES_MODE);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void txt_bus_write_C057(void *context, uint16_t address, uint8_t value) {
    txt_bus_read_C057(context, address);
}

void ds_bus_write_C00X(void *context, uint16_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    switch (address) {
        case 0xC00C:
            ds->f_80col = false;
            break;
        case 0xC00D:
            ds->f_80col = true;
            break;
    }
    update_line_mode(ds);
}

/**
 * display_state_t Class Implementation
 */
display_state_t::display_state_t() {

    for (int i = 0; i < 24; i++) {
        dirty_line[i] = 0;
    }
    display_mode = TEXT_MODE;
    display_split_mode = FULL_SCREEN;
    display_graphics_mode = LORES_MODE;
    display_page_num = DISPLAY_PAGE_1;
    display_page_table = &display_pages[display_page_num];
    flash_state = false;
    flash_counter = 0;

    buffer = new uint8_t[BASE_WIDTH * BASE_HEIGHT * sizeof(RGBA_t)];
    memset(buffer, 0, BASE_WIDTH * BASE_HEIGHT * sizeof(RGBA_t)); // TODO: maybe start it with apple logo?
}

display_state_t::~display_state_t() {
    delete[] buffer;
}


bool handle_display_event(display_state_t *ds, const SDL_Event &event) {
    SDL_Keymod mod = event.key.mod;
    SDL_Keycode key = event.key.key;

    if ((key == SDLK_KP_PLUS || key == SDLK_KP_MINUS)) {
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
        //force_display_update(ds);
        ds->video_system->set_full_frame_redraw();
        static char msgbuf[256];
        snprintf(msgbuf, sizeof(msgbuf), "Hue set to: %f, Saturation to: %f\n", config.videoHue, config.videoSaturation);
        ds->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msgbuf));
        return true;
    }
    return false;
}

/** Called by Clipboard to return current display buffer.
 * doubles scanlines and returns 2* the "native" height. */

#if 0
void display_engine_get_buffer(computer_t *computer, uint8_t *buffer, uint32_t *width, uint32_t *height) {
    display_state_t *ds = (display_state_t *)get_module_state(computer->cpu, MODULE_DISPLAY);
    // pass back the size.
    *width = BASE_WIDTH;
    *height = BASE_HEIGHT * 2;
    // BMP files have the last scanline first. What? 
    // Copy RGB values without alpha channel
    RGBA_t *src = (RGBA_t *)ds->buffer;
    uint8_t *dst = buffer;
    for (int scanline = BASE_HEIGHT - 1; scanline >= 0; scanline--) {
        for (int i = 0; i < BASE_WIDTH; i++) {
            *dst++ = src[scanline * BASE_WIDTH + i].b;
            *dst++ = src[scanline * BASE_WIDTH + i].g;
            *dst++ = src[scanline * BASE_WIDTH + i].r;
        }
        // do it again - scanline double
        for (int i = 0; i < BASE_WIDTH; i++) {
            *dst++ = src[scanline * BASE_WIDTH + i].b;
            *dst++ = src[scanline * BASE_WIDTH + i].g;
            *dst++ = src[scanline * BASE_WIDTH + i].r;
        }
    }
    static char msgbuf[256];
    snprintf(msgbuf, sizeof(msgbuf), "Screen snapshot taken");
    computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msgbuf));
}
#else
void display_engine_get_buffer(computer_t *computer, uint8_t *buffer, uint32_t *width, uint32_t *height) {
    display_state_t *ds = (display_state_t *)get_module_state(computer->cpu, MODULE_DISPLAY);
    // pass back the size.
    uint32_t w = BASE_WIDTH+7;
    *width = w;
    *height = BASE_HEIGHT * 2;
    // BMP files have the last scanline first. What? 
    // Copy RGB values without alpha channel
    //RGBA_t *src = (RGBA_t *)ds->buffer;
    uint8_t *dst = buffer;
    for (int scanline = BASE_HEIGHT - 1; scanline >= 0; scanline--) {
        ds->frame_rgba->set_line(scanline);
        for (int i = 0; i < w; i++) {
            RGBA_t pix = ds->frame_rgba->pull();
            *dst++ = pix.b;
            *dst++ = pix.g;
            *dst++ = pix.r;
        }
        // add one extra pixel to make it a multiple of 4.
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
        // do it again - scanline double
        ds->frame_rgba->set_line(scanline);
        for (int i = 0; i < w; i++) {
            RGBA_t pix = ds->frame_rgba->pull();
            *dst++ = pix.b;
            *dst++ = pix.g;
            *dst++ = pix.r;
        }
        *dst++ = 0;
        *dst++ = 0;
        *dst++ = 0;
    }
    static char msgbuf[256];
    snprintf(msgbuf, sizeof(msgbuf), "Screen snapshot taken");
    computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msgbuf));
}
#endif

// Implement the switch, but text display doesn't use it yet.
void display_write_switches(void *context, uint16_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    switch (address) {
        case 0xC00E:
            ds->f_altcharset = false;
            break;
        case 0xC00F:
            ds->f_altcharset = true;
            break;
    }
    ds->a2_display->set_char_set(ds->f_altcharset);
}

uint8_t display_read_C01E(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = (ds->f_altcharset) ? 0x80 : 0x00;
    
    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;
}

uint8_t display_read_C01F(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = (ds->f_80col) ? 0x80 : 0x00;

    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;
}

uint8_t display_read_C05EF(void *context, uint16_t address) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    update_line_mode(ds);
    ds->video_system->set_full_frame_redraw();
    return 0;
}

void display_write_C05EF(void *context, uint16_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    update_line_mode(ds);
    ds->video_system->set_full_frame_redraw();
}

uint8_t display_read_vbl(void *context, uint16_t address) {
    // This is enough to get basic VBL working. Total Replay boots anyway.
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = (ds->computer->cpu->cycles - ds->vbl_cycle_count) < 4550 ? 0x00 : 0x80;

    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : 0xEE) & 0x7F;
    return kbv | fl;

    //return iie_kb_read_strobe(ds->computer) | fl;
}

void init_mb_device_display(computer_t *computer, SlotType_t slot) {
    cpu_state *cpu = computer->cpu;
    
    // alloc and init display state
    display_state_t *ds = new display_state_t;
    video_system_t *vs = computer->video_system;

    ds->mbus = computer->mbus;
    ds->video_system = vs;
    ds->event_queue = computer->event_queue;
    ds->computer = computer;
    MMU_II *mmu = computer->mmu;
    ds->mmu = mmu;

#if 1
    // new display engine setup
    CharRom *charrom = nullptr;
    switch (computer->platform->id) {
        case PLATFORM_APPLE_IIE:
            charrom = new CharRom("assets/roms/apple2e/char.rom");
            break;
        case PLATFORM_APPLE_IIE_ENHANCED:
            charrom = new CharRom("assets/roms/apple2e_enh/char.rom");
            break;
        case PLATFORM_APPLE_II_PLUS:
            charrom = new CharRom("assets/roms/apple2_plus/char.rom");
            break;
        case PLATFORM_APPLE_II:
            charrom = new CharRom("assets/roms/apple2/char.rom");
            break;
        default:
            system_failure("Unsupported platform in display engine init");
            break;
    }
    
    ds->a2_display = new AppleII_Display(*charrom);
    uint16_t f_w = BASE_WIDTH+20;
    uint16_t f_h = BASE_HEIGHT;
    ds->frame_rgba = new(std::align_val_t(64)) Frame560RGBA(f_w, f_h);
    ds->frame_bits = new(std::align_val_t(64)) Frame560(f_w, f_h);
    ds->frame_rgba->clear(); // clear the frame buffers at startup.
    ds->frame_bits->clear();

    // Create the screen texture
    ds->screenTexture = SDL_CreateTexture(vs->renderer,
        PIXEL_FORMAT,
        SDL_TEXTUREACCESS_STREAMING,
        BASE_WIDTH+20, BASE_HEIGHT);

    if (!ds->screenTexture) {
        fprintf(stderr, "Error creating screen texture: %s\n", SDL_GetError());
    }

    SDL_SetTextureBlendMode(ds->screenTexture, SDL_BLENDMODE_NONE); /* GRRRRRRR. This was defaulting to SDL_BLENDMODE_BLEND. */
    // LINEAR gets us appropriately blurred pixels.
    // NEAREST gets us sharp pixels.
    SDL_SetTextureScaleMode(ds->screenTexture, SDL_SCALEMODE_LINEAR);

#else
    // Create the screen texture
    ds->screenTexture = SDL_CreateTexture(vs->renderer,
        PIXEL_FORMAT,
        SDL_TEXTUREACCESS_STREAMING,
        BASE_WIDTH, BASE_HEIGHT);

    if (!ds->screenTexture) {
        fprintf(stderr, "Error creating screen texture: %s\n", SDL_GetError());
    }

    SDL_SetTextureBlendMode(ds->screenTexture, SDL_BLENDMODE_NONE); /* GRRRRRRR. This was defaulting to SDL_BLENDMODE_BLEND. */
    // LINEAR gets us appropriately blurred pixels.
    // NEAREST gets us sharp pixels.
    SDL_SetTextureScaleMode(ds->screenTexture, SDL_SCALEMODE_LINEAR);

    init_displayng();
#endif

    // set in CPU so we can reference later
    set_module_state(cpu, MODULE_DISPLAY, ds);
    
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

    for (int i = 0x04; i <= 0x0B; i++) {
        mmu->set_page_shadow(i, { txt_memory_write, cpu });
    }
    for (int i = 0x20; i <= 0x5F; i++) {
        mmu->set_page_shadow(i, { hgr_memory_write, cpu });
    }

    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [ds](const SDL_Event &event) {
        return handle_display_event(ds, event);
    });

    computer->register_shutdown_handler([ds]() {
        SDL_DestroyTexture(ds->screenTexture);
        deinit_displayng();
        delete ds;
        return true;
    });

    vs->register_frame_processor(0, [cpu]() -> bool {
        return update_display_apple2(cpu);
    });

    if (computer->platform->id == PLATFORM_APPLE_IIE || computer->platform->id == PLATFORM_APPLE_IIE_ENHANCED) {
        ds->f_altcharset = false;
        ds->a2_display->set_char_set(ds->f_altcharset);
        mmu->set_C0XX_write_handler(0xC00C, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC00D, { ds_bus_write_C00X, ds });
        mmu->set_C0XX_write_handler(0xC00E, { display_write_switches, ds });
        mmu->set_C0XX_write_handler(0xC00F, { display_write_switches, ds });
        mmu->set_C0XX_read_handler(0xC01E, { display_read_C01E, ds });
        mmu->set_C0XX_read_handler(0xC01F, { display_read_C01F, ds });
        mmu->set_C0XX_read_handler(0xC05E, { display_read_C05EF, ds });
        mmu->set_C0XX_write_handler(0xC05E, { display_write_C05EF, ds });
        mmu->set_C0XX_read_handler(0xC05F, { display_read_C05EF, ds });
        mmu->set_C0XX_write_handler(0xC05F, { display_write_C05EF, ds });
        mmu->set_C0XX_read_handler(0xC019, { display_read_vbl, ds });
        computer->register_reset_handler([ds]() {
            ds->f_80col = false;
            ds->f_double_graphics = true;
            ds->f_altcharset = false;
            ds->a2_display->set_char_set(ds->f_altcharset);
            update_line_mode(ds);
            return true;
        });
    }
}

void display_dump_file(cpu_state *cpu, const char *filename, uint16_t base_addr, uint16_t sizer) {
    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open %s for writing\n", filename);
        return;
    }
    // Write 8192 bytes (0x2000) from memory starting at base_addr
    for (uint16_t offset = 0; offset < sizer; offset++) {
        uint8_t byte = cpu->mmu->read(base_addr + offset);
        fwrite(&byte, 1, 1, fp);
    }
    fclose(fp);
}

void display_dump_hires_page(cpu_state *cpu, int page) {
    uint16_t base_addr = (page == 1) ? 0x2000 : 0x4000;
    display_dump_file(cpu, "dump.hgr", base_addr, 0x2000);
    fprintf(stdout, "Dumped HGR page %d to dump.hgr\n", page);
}

void display_dump_text_page(cpu_state *cpu, int page) {
    uint16_t base_addr = (page == 1) ? 0x0400 : 0x0800;
    display_dump_file(cpu, "dump.txt", base_addr, 0x0400);
    fprintf(stdout, "Dumped TXT page %d to dump.txt\n", page);
}
