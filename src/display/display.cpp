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
#include "text_40x24.hpp"
//#include "hgr_280x192.hpp"
#include "platforms.hpp"

#include "util/dialog.hpp"

#include "display/ntsc.hpp"

#include "videosystem.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/VideoScanGenerator.hpp"
#include "mbus/MessageBus.hpp"
#include "mbus/KeyboardMessage.hpp"

#include "devices/displaypp/VideoScanGenerator.cpp"
#include "devices/displaypp/VideoScannerIIgs.hpp"
#include "devices/displaypp/VideoScannerIIe.hpp"
#include "devices/displaypp/VideoScannerIIePAL.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/AppleIIgsColors.hpp"

#include "util/DebugHandlerIDs.hpp"

static constexpr const RGBA_t (&gs_text_colors)[16] = AppleIIgs::TEXT_COLORS;

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



/*
border texture is laid out based on the hc/vc positions. i.e
   0-6: right border
   7-12: left border
   13-52: top/bottom border center content

*/ 
void calculate_border_rects(display_state_t *ds, bool shift_enabled) {
    float shift_offset = shift_enabled ? 7.0f : 0.0f;
    float width = shift_enabled ? 567.0f : 560.0f;

    constexpr float b_l_x = 7.0f;
    constexpr float b_l_w = 6.0f;

    constexpr float b_r_x = 0.0f;
    constexpr float b_r_w = 7.0f;

    border_rect_array_t &ii_borders = ds->ii_borders;
    // top
    ii_borders[B_TOP][B_LT].src = {b_l_x, 243.0, b_l_w, 19};
    ii_borders[B_TOP][B_LT].dst = {0.0, 0.0, 42.0, 19};

    ii_borders[B_TOP][B_CEN].src = {13, 243.0, 40, 19};
    ii_borders[B_TOP][B_CEN].dst = {42, 0.0, 560, 19};

    ii_borders[B_TOP][B_RT].src = {0, 243.0, b_r_w, 19};
    ii_borders[B_TOP][B_RT].dst = {42.0f+560.0f-shift_offset, 0.0, 49.0, 19};

    // center
    ii_borders[B_CEN][B_LT].src = {b_l_x, 0.0, b_l_w, 192};
    ii_borders[B_CEN][B_LT].dst = {0, 19.0, 42.0, 192};

    ii_borders[B_CEN][B_CEN].src = {0.0, 0.0, width, (float)192}; // not from border texture
    ii_borders[B_CEN][B_CEN].dst = {42.0f-shift_offset, 19.0, width, 192}; // not from border texture

    ii_borders[B_CEN][B_RT].src = {0.0, 0.0, b_r_w, 192};
    ii_borders[B_CEN][B_RT].dst = {42.0f+560.0f-shift_offset, 19.0, 49.0, 192};

    // bottom
    ii_borders[B_BOT][B_LT].src = {b_l_x, 192.0, b_l_w, 21};
    ii_borders[B_BOT][B_LT].dst = {0.0, 19+192, 42.0, 21};

    ii_borders[B_BOT][B_CEN].src = {13.0, 192.0, 40, 21};
    ii_borders[B_BOT][B_CEN].dst = {42, 19+192, 560, 21};

    ii_borders[B_BOT][B_RT].src = {0, 192.0, b_r_w, 21};
    ii_borders[B_BOT][B_RT].dst = {42.0f+560.0f-shift_offset, 19+192, 49.0, 21};

    // SHR mode - in shr mode one cycle = 8 pixels. So we have to scale the borders out accordingly.
    border_rect_array_t &shr_borders = ds->shr_borders;
    // top
    shr_borders[B_TOP][B_LT].src = {b_l_x, 243.0, b_l_w, 19};
    shr_borders[B_TOP][B_LT].dst = {0.0, 0.0, 48.0, 19};

    shr_borders[B_TOP][B_CEN].src = {13, 243.0, 40, 19};
    shr_borders[B_TOP][B_CEN].dst = {48, 0.0, 640, 19};

    shr_borders[B_TOP][B_RT].src = {0, 243.0, b_r_w, 19};
    shr_borders[B_TOP][B_RT].dst = {48.0f+640.0f, 0.0, 56.0, 19};
    
    // center
    shr_borders[B_CEN][B_LT].src = {0.0, 0.0, b_l_w, 192};
    shr_borders[B_CEN][B_LT].dst = {0.0, 19.0, 48.0, 200};

    shr_borders[B_CEN][B_CEN].src = {0.0f, 0.0f, 640.0f, 200.0f};
    shr_borders[B_CEN][B_CEN].dst = {48.0f, 19.0f, 640.0f, 200.0f};

    shr_borders[B_CEN][B_RT].src = {0.0, 0.0, b_r_w, 192};
    shr_borders[B_CEN][B_RT].dst = {48+640, 19.0, 56.0, 200}; 

    // bottom
    shr_borders[B_BOT][B_LT].src = {b_l_x, 192.0, b_l_w, 21};
    shr_borders[B_BOT][B_LT].dst = {0.0, 19+192, 48.0, 21};

    shr_borders[B_BOT][B_CEN].src = {13.0, 192.0, 40, 21};
    shr_borders[B_BOT][B_CEN].dst = {48, 19+192, 640, 21};

    shr_borders[B_BOT][B_RT].src = {0, 192.0, b_r_w, 21};
    shr_borders[B_BOT][B_RT].dst = {48.0f+640.0f, 19+192, 56.0, 21};   
}

void print_rect(const char *name, border_rect_t &r) {
    printf("%s: SRC: (%f, %f, %f, %f)\n", name, r.src.x, r.src.y, r.src.w, r.src.h);
    printf("%s: DST: (%f, %f, %f, %f)\n", name, r.dst.x, r.dst.y, r.dst.w, r.dst.h);
}

void print_border_rects(display_state_t *ds) {
    print_rect("ii_borders[B_CEN][B_CEN]", ds->ii_borders[B_CEN][B_CEN]);
}

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

/**
 * This is effectively a "redraw the entire screen each frame" method now.
 * With an optimization only update dirty lines.
 */
bool update_display_apple2(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

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
    if (ds->display_page_num == DISPLAY_PAGE_1 || ds->a2_display->is_80store()) {
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
    uint8_t *shr_page = ram + 0x12000;

    int updated = 0;

    SDL_Texture *stage2 = ds->stage2;
    SDL_Renderer *renderer = vs->renderer;

    bool video_mode_is_shr = (ds->new_video & 0x80) != 0;
    border_rect_array_t &modes_rects = (video_mode_is_shr) ? ds->shr_borders : ds->ii_borders;

    SDL_SetRenderTarget(renderer, stage2);
    SDL_RenderClear(renderer);
    
    // first check - borders or no.
    if (ds->video_scanner_type == Scanner_AppleIIgs) {
        // draw borders using rectangles.
        RGBA_t border_color = gs_text_colors[ds->border_color];
        SDL_SetRenderDrawColor(vs->renderer, border_color.r, border_color.g, border_color.b, border_color.a);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_TOP][B_LT].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_TOP][B_CEN].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_TOP][B_RT].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_CEN][B_LT].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_CEN][B_RT].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_BOT][B_LT].dst);
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_BOT][B_CEN].dst); 
        SDL_RenderFillRect(vs->renderer, &modes_rects[B_BOT][B_RT].dst);
    }

    // second check - SHR vs II
    // we are still just creating the texture

    if (ds->new_video & 0x80) {
        ds->fr_shr->open();
        ds->a2_display->generate_shr((SHR *)shr_page, ds->fr_shr);
        ds->fr_shr->close();
        ds->video_system->clear();
        // copy it into the stage2 texture
        SDL_RenderTexture(renderer, ds->fr_shr->get_texture(), &modes_rects[B_CEN][B_CEN].src, &modes_rects[B_CEN][B_CEN].dst);
    } else {

        for (uint16_t line = 0; line < 24; line++) {
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
                case LM_HIRES_MODE_NOSHIFT:
                    ds->a2_display->generate_hires40_noshift(hgr_page, ds->frame_bits, line);
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
        }

        RGBA_t mono_color_value = vs->get_mono_color();
        ds->frame_rgba->open();
        // do a switch on display engine later..
        switch (vs->display_color_engine) {
            case DM_ENGINE_NTSC:
                if (ds->display_mode == TEXT_MODE) {
                    ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF));
                } else {
                    ds->mon_ntsc.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF) /* , 1 */);
                }
                break;
            case DM_ENGINE_RGB:
                ds->mon_rgb.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF) /* , 1 */);
                break;
            case DM_ENGINE_MONO:
                ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, mono_color_value);
                break;
            default:
                break; // never happens
        }
        ds->frame_rgba->close();
        
        // copy it into the stage2 texture.
        SDL_RenderTexture(renderer, ds->screenTexture, &modes_rects[B_CEN][B_CEN].src, &modes_rects[B_CEN][B_CEN].dst);
    }
    // back to main renderer.
    SDL_SetRenderTarget(renderer, nullptr);
    // and emit.
    vs->render_frame(stage2, (video_mode_is_shr) ? &ds->gs_shr_frame_src : &ds->gs_ii_frame_src, &ds->frame_dst);
    vs->force_full_frame_redraw = false;

    return true;
}


bool update_display_apple2_cycle(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    ScanBuffer *frame_scan = ds->video_scanner->get_frame_scan();
    ds->vsg->generate_frame(frame_scan, ds->frame_bits);
    ds->frame_rgba->open();
    switch (vs->display_color_engine) {
        case DM_ENGINE_MONO:
            ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, vs->get_mono_color());
            break;
        case DM_ENGINE_NTSC:
            ds->mon_ntsc.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)  /* , 1 */);
            break;
        case DM_ENGINE_RGB: // we send a green value here but mon_rgb does not use it.
            ds->mon_rgb.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF) /* , 1 */);
            break;
        default:
            break;
    }
    ds->frame_rgba->close();
     
    vs->render_frame(ds->screenTexture, 
        &ds->ii_borders[B_CEN][B_CEN].src, 
        &ds->ii_borders[B_CEN][B_CEN].dst
    );

    return true;
}

bool update_display_apple2gs_cycle(cpu_state *cpu) {
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    video_system_t *vs = ds->video_system;

    ScanBuffer *frame_scan = ds->video_scanner->get_frame_scan();
    ds->fr_border->open();
    ds->fr_shr->open();
    ds->vsg->generate_frame(frame_scan, ds->frame_bits, ds->fr_border, ds->fr_shr);
    ds->fr_border->close();
    ds->fr_shr->close();

    if (!(ds->new_video & 0x80)) {
        ds->frame_rgba->open();
        switch (vs->display_color_engine) {
            case DM_ENGINE_MONO:
                ds->mon_mono.render(ds->frame_bits, ds->frame_rgba, vs->get_mono_color());
                break;
            case DM_ENGINE_NTSC:
                ds->mon_ntsc.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF)  /* , 1 */);
                break;
            case DM_ENGINE_RGB: // we send a green value here but mon_rgb does not use it.
                ds->mon_rgb.render(ds->frame_bits, ds->frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF) /* , 1 */);
                break;
            default:
                break;
        }
        ds->frame_rgba->close();
    }

    // TODO: vs->render_frame(.. )  with the border

    SDL_Renderer *renderer = vs->renderer;
    SDL_Texture *stage2 = ds->stage2;
    SDL_Texture *txt_border = ds->fr_border->get_texture();
    SDL_Texture *txt_shr = ds->fr_shr->get_texture();
    SDL_Texture *txt_apple2 = ds->screenTexture;

    bool video_mode_is_shr = (ds->new_video & 0x80) != 0;
    SDL_SetRenderTarget(renderer, stage2);
    SDL_RenderClear(renderer);
    border_rect_array_t &modes_rects = (video_mode_is_shr) ? ds->shr_borders : ds->ii_borders;

    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_TOP][B_LT].src, &modes_rects[B_TOP][B_LT].dst); // top left
    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_TOP][B_CEN].src, &modes_rects[B_TOP][B_CEN].dst); // top
    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_TOP][B_RT].src, &modes_rects[B_TOP][B_RT].dst); // top right

    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_CEN][B_LT].src, &modes_rects[B_CEN][B_LT].dst); // left
    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_CEN][B_RT].src, &modes_rects[B_CEN][B_RT].dst); // right

    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_BOT][B_LT].src, &modes_rects[B_BOT][B_LT].dst); // bottom left
    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_BOT][B_CEN].src, &modes_rects[B_BOT][B_CEN].dst); // bottom
    SDL_RenderTexture(renderer, txt_border, &modes_rects[B_BOT][B_RT].src, &modes_rects[B_BOT][B_RT].dst); // bottom right
    // Copy the content texture in
    if (video_mode_is_shr) {
        SDL_RenderTexture(renderer, txt_shr, &modes_rects[B_CEN][B_CEN].src, &modes_rects[B_CEN][B_CEN].dst);
    } else {
        // draw over border but shiftable portions need to be alpha'd with border color.
        SDL_RenderTexture(renderer, txt_apple2, &modes_rects[B_CEN][B_CEN].src, &modes_rects[B_CEN][B_CEN].dst);                
    }
    SDL_SetRenderTarget(renderer, nullptr);
    vs->render_frame(stage2, (video_mode_is_shr) ? &ds->gs_shr_frame_src : &ds->gs_ii_frame_src, &ds->frame_dst);

    return true;
}

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
            if (ds->f_80col) {
                if (!ds->f_double_graphics) {
                    top_mode = LM_HIRES80_MODE;
                } else {
                    top_mode = LM_HIRES_MODE;
                }
            } else {
                if (!ds->f_double_graphics) {
                    top_mode = LM_HIRES_MODE_NOSHIFT;
                } else {
                    top_mode = LM_HIRES_MODE;
                }
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

uint8_t txt_bus_read_C050(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set graphics mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Graphics Mode\n");
    set_display_mode(ds, GRAPHICS_MODE);
    if (!ds->framebased) ds->video_scanner->set_graf();
    ds->video_system->set_full_frame_redraw();
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
    if (!ds->framebased) ds->video_scanner->set_text();
    ds->video_system->set_full_frame_redraw();
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
    if (!ds->framebased) ds->video_scanner->set_full();
    ds->video_system->set_full_frame_redraw();
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
    if (!ds->framebased) ds->video_scanner->set_mixed();
    ds->video_system->set_full_frame_redraw();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C053(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C053(context, address);
}


uint8_t txt_bus_read_C054(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 1
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to screen 1\n");
    set_display_page1(ds);
    if (!ds->framebased) ds->video_scanner->set_page_1();
    ds->video_system->set_full_frame_redraw();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C054(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C054(context, address);
}

uint8_t txt_bus_read_C055(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // switch to screen 2
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Switching to screen 2\n");
    set_display_page2(ds);
    if (!ds->framebased) ds->video_scanner->set_page_2();
    ds->video_system->set_full_frame_redraw();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C055(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C055(context, address);
}


uint8_t txt_bus_read_C056(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set lo-res (graphics) mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Lo-Res Mode\n");
    set_graphics_mode(ds, LORES_MODE);
    if (!ds->framebased) ds->video_scanner->set_lores();
    ds->video_system->set_full_frame_redraw();
    return ds->mmu->floating_bus_read();
}

void txt_bus_write_C056(void *context, uint32_t address, uint8_t value) {
    txt_bus_read_C056(context, address);
}

uint8_t txt_bus_read_C057(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    // set hi-res (graphics) mode
    if (DEBUG(DEBUG_DISPLAY)) fprintf(stdout, "Set Hi-Res Mode\n");
    set_graphics_mode(ds, HIRES_MODE);
    if (!ds->framebased) ds->video_scanner->set_hires();
    ds->video_system->set_full_frame_redraw();
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
            ds->a2_display->set_80store(false);
            return;
        case 0xC001:
            ds->video_scanner->set_80store(true);
            ds->a2_display->set_80store(true);
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
    if (!ds->framebased) ds->video_scanner->set_80col_f(ds->f_80col);
    update_line_mode(ds);
    force_display_update(ds);
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
    delete vsg;
    delete a2_display;
    delete frame_rgba;
    delete frame_bits;
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
        //force_display_update(ds);
        ds->video_system->set_full_frame_redraw();
        static char msgbuf[256];
        snprintf(msgbuf, sizeof(msgbuf), "Hue set to: %f, Saturation to: %f\n", config.videoHue, config.videoSaturation);
        ds->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msgbuf));
        return true;
    }
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
    return false;
}

/** Called by Clipboard to return current display buffer.
 * doubles scanlines and returns 2* the "native" height. */

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
    ds->a2_display->set_normal_alt(ds->f_altcharset);
}

/**
 * IIGS Video Control Registers
 */
uint8_t display_read_C029(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->new_video;
}

void display_write_C029(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->new_video = value;
    if (ds->new_video & 0x80) {
        ds->video_scanner->set_shr();
        // TODO:ds->a2_display->set_shr(true);
    } else {
        ds->video_scanner->reset_shr();
        // TODO: ds->a2_display->reset_shr();
    }
    if (ds->new_video & 0x20) {
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

void display_write_C022(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->text_color = value;
    ds->video_scanner->set_text_bg(value & 0x0F);
    ds->video_scanner->set_text_fg(value >> 4);
    ds->a2_display->set_text_fg(value >> 4);
    ds->a2_display->set_text_bg(value & 0x0F);
    // TODO: also set in AppleII_Display
}

// TODO: this register is split between realtime clock and border color. and needs to override speaker.
uint8_t display_read_C034(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    return ds->border_color;
}

void display_write_C034(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->border_color = value & 0x0F;
    ds->video_scanner->set_border_color(value);
    ds->a2_display->set_border_color(value);
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

/* uint8_t display_read_C01E(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = (ds->f_altcharset) ? 0x80 : 0x00;
    
    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : ds->mmu->floating_bus_read()) & 0x7F;
    return kbv | fl;
}

uint8_t display_read_C01F(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    uint8_t fl = (ds->f_80col) ? 0x80 : 0x00;

    KeyboardMessage *keymsg = (KeyboardMessage *)ds->mbus->read(MESSAGE_TYPE_KEYBOARD);
    uint8_t kbv = (keymsg ? keymsg->mk->last_key_val : ds->mmu->floating_bus_read()) & 0x7F;
    return kbv | fl;
} */

uint8_t display_read_C05EF(void *context, uint32_t address) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    ds->video_scanner->set_dblres_f(!ds->f_double_graphics);
    update_line_mode(ds);
    ds->video_system->set_full_frame_redraw();
    return ds->mmu->floating_bus_read();
}

void display_write_C05EF(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_double_graphics = (address & 0x1); // this is inverted sense
    ds->video_scanner->set_dblres_f(!ds->f_double_graphics);
    update_line_mode(ds);
    ds->video_system->set_full_frame_redraw();
}

/* VBL, Mouse, 1 sec, 1/4 sec Interrupt Handling Section - IIgs specific registers */

void display_write_c023(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    
    ds->f_VGCINT = (ds->f_VGCINT & 0b1111'1000) | (value & 0b0000'0111);

    //ds->f_VGCINT = value;

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

/* C02B - LANGSEL - IIgs specific */
void display_write_C02B(void *context, uint32_t address, uint8_t value) {
    display_state_t *ds = (display_state_t *)context;
    ds->f_langsel = value & 0b1111'1000;
    // TODO: set language for display. Only values 0-7 are valid.
    ds->a2_display->set_char_set((ds->f_langsel & 0xE0) >> 5); // set LS scanner.
    ds->vsg->set_char_set((ds->f_langsel & 0xE0) >> 5); // set LS scanner.
    // TODO: set video mode timing ntsc vs pal.
    // TODO: implement LANGUAGE switch (if 0, use lang 0. Otherwise use whatever lang selected.)
    /* The Apple IIgs Firmware Reference states that LANGSEL bit 3 is "0 if primary lang set selected", but this appears to be incorrect.
     Bit 3 is set to 1 by BRAM restore during power-on (or booting GS/OS, or entering the Control Panel) and hardware testing shows that 
     the language in bits 5-7 is ignored when bit 3 is 0.    */
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

void display_update_video_scanner(display_state_t *ds, cpu_state *cpu) {
    if (ds->clock->get_clock_mode() == CLOCK_FREE_RUN) {
        ds->framebased = true;
        ds->clock->set_video_scanner(nullptr);
        /* for (int i = 0x04; i <= 0x0B; i++) {
            ds->mmu->set_page_shadow(i, { txt_memory_write, cpu });
        }
        for (int i = 0x20; i <= 0x5F; i++) {
            ds->mmu->set_page_shadow(i, { hgr_memory_write, cpu });
        } */
    } else {
        ds->framebased = false;
        ds->clock->set_video_scanner(ds->video_scanner);
        for (int i = 0x04; i <= 0x0B; i++) {
            ds->mmu->set_page_shadow(i, { nullptr, cpu });
        }
        for (int i = 0x20; i <= 0x5F; i++) {
            ds->mmu->set_page_shadow(i, { nullptr, cpu });
        }
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
            
            // every 60 frames, also assert 1 sec
            if (++ds->onesec_counter == 60) {
                ds->f_onesec_asserted = true; // only assert if enabled.
                ds->onesec_counter = 0; // kinda need to reset the counter bro.
                update_vgc_interrupt(ds, true);
            }
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
    df->addLine("  Framebased: %d", ds->framebased);
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
    switch (computer->platform->id) {
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
    }
    
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

    // Initialize the VideoScanGenerator with the CharRom
    ds->vsg = new VideoScanGenerator(charrom);

    ds->char_rom = charrom;
    ds->a2_display = new AppleII_Display(charrom);  // Create the full-frame engine.
    
    // Create the screen textures

    uint16_t f_w = BASE_WIDTH+20;
    uint16_t f_h = BASE_HEIGHT;
    //ds->frame_rgba = new(std::align_val_t(64)) Frame560RGBA(567, f_h, ds->screenTexture);
    ds->frame_rgba = new(std::align_val_t(64)) Frame560RGBA(567, f_h, vs->renderer, PIXEL_FORMAT);
    ds->screenTexture = ds->frame_rgba->get_texture();
    ds->frame_bits = new(std::align_val_t(64)) Frame560(560, f_h);
    //ds->frame_rgba->clear(RGBA_t::make(0, 0, 0, 0)); // clear the frame buffers at startup.
    //ds->frame_bits->clear(0);

    ds->stage2 = SDL_CreateTexture(vs->renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_TARGET, 768, 256);
    if (!ds->stage2) {
        printf("Failed to create txt_shr\n");
        printf("SDL Error: %s\n", SDL_GetError());
        system_failure("Failed to create stage2 texture");
    }

    // LINEAR gets us appropriately blurred pixels, NEAREST gets us sharp pixels, PIXELART is sharper pixels that are more accurate
    SDL_SetTextureBlendMode(ds->screenTexture, SDL_BLENDMODE_NONE); /* GRRRRRRR. This was defaulting to SDL_BLENDMODE_BLEND. */
    SDL_SetTextureScaleMode(ds->screenTexture, SDL_SCALEMODE_NEAREST);

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

    display_update_video_scanner(ds, cpu);

    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [ds](const SDL_Event &event) {
        return handle_display_event(ds, event);
    });

    computer->register_shutdown_handler([ds]() {
        //SDL_DestroyTexture(ds->screenTexture);
        
        //deinit_displayng();
        delete ds;
        return true;
    });

    if (computer->platform->id == PLATFORM_APPLE_IIE || computer->platform->id == PLATFORM_APPLE_IIE_ENHANCED
    || computer->platform->id == PLATFORM_APPLE_IIE_65816 || computer->platform->id == PLATFORM_APPLE_IIGS) {
        ds->f_altcharset = false;
        ds->a2_display->set_char_set(ds->f_altcharset);
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
        computer->register_reset_handler([ds]() {
            if (ds->computer->platform->id == PLATFORM_APPLE_IIGS) {
                display_write_c041(ds, 0xC041, 0x00);
            }
            ds->f_80col = false;
            ds->f_double_graphics = true;
            ds->f_altcharset = false;
            ds->video_scanner->reset_80col();
            ds->video_scanner->reset_altchrset();
            ds->video_scanner->reset_dblres();
            ds->a2_display->set_char_set(ds->f_altcharset);
            ds->a2_display->set_80store(false); // TODO: check this, but it makes sense.
            // TODO: this is the cleanest way to do it for now, but it feels a little hacky, as if
            // reset handler in mmu and here should each be responsible for clearing their own bits.
            ds->mmu->write(0xC029, ds->new_video&0x1);

            update_line_mode(ds);
            return true;
        });
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

    switch (ds->video_scanner_type) {
        case Scanner_AppleIIgs:
        case Scanner_AppleII:
            calculate_border_rects(ds, false);
            break;
        case Scanner_AppleIIe:
        case Scanner_AppleIIePAL:
            calculate_border_rects(ds, true);
            break;
        default:
            system_failure("Unsupported video scanner type in display engine init");
            break;
    }

    if (computer->platform->id == PLATFORM_APPLE_IIE_65816 || computer->platform->id == PLATFORM_APPLE_IIGS) {
        // allocate border and shr frame buffers.
        ds->fr_border = new(std::align_val_t(64)) FrameBorder(53, 263, vs->renderer, PIXEL_FORMAT);
        ds->fr_shr = new(std::align_val_t(64)) Frame640(640, 200, vs->renderer, PIXEL_FORMAT);
        SDL_SetTextureScaleMode(ds->fr_border->get_texture(), SDL_SCALEMODE_NEAREST);
        SDL_SetTextureScaleMode(ds->fr_shr->get_texture(), SDL_SCALEMODE_NEAREST);


        mmu->set_C0XX_read_handler(0xC02E, { display_read_C02EF, ds });
        mmu->set_C0XX_read_handler(0xC02F, { display_read_C02EF, ds });

        mmu->set_C0XX_read_handler(0xC029, { display_read_C029, ds });
        mmu->set_C0XX_write_handler(0xC029, { display_write_C029, ds });
        mmu->set_C0XX_read_handler(0xC022, { display_read_C022, ds });
        mmu->set_C0XX_write_handler(0xC022, { display_write_C022, ds });
        mmu->set_C0XX_read_handler(0xC034, { display_read_C034, ds });
        mmu->set_C0XX_write_handler(0xC034, { display_write_C034, ds });
        ds->vsg->set_display_shift(false); // no shift in Apple IIgs mode.
        ds->mon_mono.set_shift_enabled(false);
        ds->mon_ntsc.set_shift_enabled(false);
        ds->mon_rgb.set_shift_enabled(false);
        
        // Set default video scanner colors for Apple IIgs. (F, 6, 6)
        ds->text_color = 0xF6; ds->video_scanner->set_text_fg(0x0F); ds->video_scanner->set_text_bg(0x06);
        ds->a2_display->set_text_fg(0x0F); ds->a2_display->set_text_bg(0x06);
        ds->border_color = 0x06; ds->video_scanner->set_border_color(0x06);
        ds->a2_display->set_border_color(0x06);
    }

    if (ds->video_scanner_type == Scanner_AppleIIgs) {
        vs->register_frame_processor(0, [ds, cpu](bool force_full_frame) -> bool {
            bool ret;
            if (ds->framebased || force_full_frame) {
                update_flash_state(cpu);
                ret = update_display_apple2(cpu);
            } else {
                ret = update_display_apple2gs_cycle(cpu);
            }
            return ret;
        });
    } else {
        vs->register_frame_processor(0, [ds, cpu](bool force_full_frame) -> bool {
            bool ret;
            if (ds->framebased || force_full_frame) {
                update_flash_state(cpu);
                ret = update_display_apple2(cpu);
            } else {
                ret = update_display_apple2_cycle(cpu);
            }
            return ret;
        });
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

void init_mb_device_display_frameonly(computer_t *computer, SlotType_t slot) {
    init_mb_device_display_common(computer, slot, false);
}


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
