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

#include <stdexcept>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>

#include "cpu.hpp"
#include "DiskII_Button.hpp"
#include "Unidisk_Button.hpp"
#include "MousePositionTile.hpp"
#include "Container.hpp"
#include "AssetAtlas.hpp"
#include "Style.hpp"
#include "MainAtlas.hpp"
#include "OSD.hpp"
#include "display/display.hpp"
#include "util/mount.hpp"
#include "util/reset.hpp"
#include "util/soundeffects.hpp"
#include "ModalContainer.hpp"
#include "util/strndup.h"

// we need to use data passed to us, and pass it to the ShowOpenFileDialog, so when the file select event
// comes back later, we know which drive this was for.
// TODO: only allow one of these to be open at a time. If one is already open, disregard.

struct diskii_callback_data_t {
    OSD *osd;
    uint64_t key;
};

struct diskii_modal_callback_data_t {
    OSD *osd;
    ModalContainer_t *container;
    uint64_t key;
};

/** handle file dialog callback */
static void /* SDLCALL */ file_dialog_callback(void* userdata, const char* const* filelist, int filter)
{
     diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;

    OSD *osd = data->osd;
    osd->set_raise_window();

    if (filelist[0] == nullptr) return; // user cancelled dialog

    // returns callback: /Users/bazyar/src/AppleIIDisks/33master.dsk when selecting
    // a disk image file.
    printf("file_dialog_callback: %s\n", filelist[0]);
    // 1. unmount current image (if present).
    // 2. mount new image.
    // TODO: this is never called here since we catch "mounted and want to unmount below in diskii_button_click"
    /* drive_status_t ds = osd->cpu->mounts->media_status(data->key);
    if (ds.is_mounted) {
        osd->cpu->mounts->unmount_media(data->key);
        // shouldn't need soundeffect here, we play it elsewhere.
    } */

    disk_mount_t dm;
    dm.filename = strndup(filelist[0], 1024);
    dm.slot = data->key >> 8;
    dm.drive = data->key & 0xFF;   
    osd->cpu->mounts->mount_media(dm);
    osd->cpu->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_CLOSE));
}

void diskii_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    OSD *osd = data->osd;

    if (osd->cpu->mounts->media_status(data->key).is_mounted) {
        // if media was modified, create Event to handle modal dialog. Otherwise, just unmount.
        if (osd->cpu->mounts->media_status(data->key).is_modified) {
            osd->show_diskii_modal(data->key, 0);
        } else {
            //disk_mount_t dm;    
            osd->cpu->mounts->unmount_media(data->key, DISCARD);
            osd->cpu->event_queue->addEvent(new Event(EVENT_PLAY_SOUNDEFFECT, 0, SE_SHUGART_OPEN));
        }
        return;
    }

    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "do;po;woz;dsk;hdv;2mg" },
        { "All files",   "*" }
    };

    printf("diskii button clicked\n");
    SDL_ShowOpenFileDialog(file_dialog_callback, 
        userdata, 
        osd->get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        nullptr,
        false);
}

void unidisk_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    OSD *osd = data->osd;

    if (osd->cpu->mounts->media_status(data->key).is_mounted) {
        disk_mount_t dm;
        osd->cpu->mounts->unmount_media(data->key, DISCARD); // TODO: we write blocks as we go, there is nothing to 'save' here.
        return;
    }
    
    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "po;dsk;hdv;2mg" },
        { "All files",   "*" }
    };

    printf("unidisk button clicked\n");
    SDL_ShowOpenFileDialog(file_dialog_callback, 
        userdata, 
        osd->get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        nullptr,
        false);
}

void set_color_display_ntsc(void *data) {
    printf("set_color_display_ntsc %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_display_engine(cpu, DM_ENGINE_NTSC);
}

void set_color_display_rgb(void *data) {
    printf("set_color_display_rgb %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_display_engine(cpu, DM_ENGINE_RGB);
}

void set_green_display(void *data) {
    printf("set_green_display %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_display_mono_color(cpu, DM_MONO_GREEN);
    set_display_engine(cpu, DM_ENGINE_MONO);
}

void set_amber_display(void *data) {
    printf("set_amber_display %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_display_mono_color(cpu, DM_MONO_AMBER);
    set_display_engine(cpu, DM_ENGINE_MONO);
}

void set_white_display(void *data) {
    printf("set_white_display %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_display_mono_color(cpu, DM_MONO_WHITE);
    set_display_engine(cpu, DM_ENGINE_MONO);
}

void set_mhz_1_0(void *data) {
    printf("set_mhz_1_0 %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_1_024MHZ);
}

void set_mhz_2_8(void *data) {
    printf("set_mhz_2_8 %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_2_8MHZ);
}

void set_mhz_4_0(void *data) {
    printf("set_mhz_4_0 %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_4MHZ);
}

void set_mhz_infinity(void *data) {
    printf("set_mhz_infinity %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_FREE_RUN);
}

void click_reset_cpu(void *data) {
    printf("click_reset_cpu %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    system_reset(cpu, false);
}

void modal_diskii_click(void *data) {
    diskii_modal_callback_data_t *d = (diskii_modal_callback_data_t *)data;
    printf("modal_diskii_click %p %lld\n", data, d->key);
    OSD *osd = d->osd;
    cpu_state *cpu = osd->cpu;
    ModalContainer_t *container = d->container;
    cpu->event_queue->addEvent(new Event(EVENT_MODAL_CLICK, container->get_key(), d->key));
    // I need to reference back to the button that was clicked and get its ID.
}


/** -------------------------------------------------------------------------------------------------- */

SDL_Window* OSD::get_window() { 
    return window; 
}

void OSD::set_raise_window() {
    cpu->event_queue->addEvent(new Event(EVENT_REFOCUS, 0, 0));
}

OSD::OSD(cpu_state *cpu, SDL_Renderer *rendererp, SDL_Window *windowp, SlotManager_t *slot_manager, int window_width, int window_height) 
    : renderer(rendererp), window(windowp), window_w(window_width), window_h(window_height), cpu(cpu), slot_manager(slot_manager) {

    cpTexture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        window_w, 
        window_h
    );

    if (!cpTexture) {
        throw std::runtime_error(std::string("Error creating cpTexture: ") + SDL_GetError());
    }

    /* Setup a text renderer for this OSD */
    text_render = new TextRenderer(rendererp, "fonts/OpenSans-Regular.ttf", 15.0f);

    Style_t CS;
    CS.padding = 4;
    CS.border_width = 2;
    CS.background_color = 0xFFFFFFFF;
    CS.border_color = 0x008000E0;
    CS.hover_color = 0x008080FF;
    Style_t DS;
    DS.background_color = 0x00000000;
    DS.border_color = 0xFFFFFFFF;
    DS.hover_color = 0x008080FF;
    DS.padding = 5;
    DS.border_width = 2;
    Style_t SC;
    SC.background_color = 0x800080FF;
    SC.border_color = 0xFFFFFFFF;
    SC.border_width = 2;
    SC.hover_color = 0x008080FF;
    SC.padding = 4;
    Style_t SS;
    SS.background_color = 0x0084C6FF;
    SS.border_color = 0xFFFFFFFF;
    SS.hover_color = 0x606060FF;
    SS.text_color = 0xFFFFFFFF;
    SS.padding = 4;
    SS.border_width = 1;
    Style_t HUD;
    HUD.background_color = 0x00000000;
    HUD.border_color = 0x000000FF;
    HUD.border_width = 0;

    aa = new AssetAtlas_t(renderer, "img/atlas.png");
    aa->set_elements(MainAtlas_count, asset_rects);

    // Create a container for our drive buttons
    Container_t *drive_container = new Container_t(renderer, 10, CS);  // Increased to 5 to accommodate the mouse position tile
    drive_container->set_position(600, 70);
    drive_container->set_size(415, 600);
    containers.push_back(drive_container);

    // TODO: create buttons based on what is in slots.
    // Create the buttons
    diskii_button1 = new DiskII_Button_t(aa, DiskII_Open, DS); // this needs to have our disk key . or alternately use a different callback.
    diskii_button1->set_key(0x600);
    diskii_button1->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, 0x600});

    diskii_button2 = new DiskII_Button_t(aa, DiskII_Closed, DS);
    diskii_button2->set_key(0x601);
    diskii_button2->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, 0x601});

    unidisk_button1 = new Unidisk_Button_t(aa, Unidisk_Face, DS); // this needs to have our disk key . or alternately use a different callback.
    unidisk_button1->set_key(0x500);
    unidisk_button1->set_click_callback(unidisk_button_click, new diskii_callback_data_t{this, 0x500});

    unidisk_button2 = new Unidisk_Button_t(aa, Unidisk_Face, DS); // this needs to have our disk key . or alternately use a different callback.
    unidisk_button2->set_key(0x501);
    unidisk_button2->set_click_callback(unidisk_button_click, new diskii_callback_data_t{this, 0x501});

    // Add buttons to container
    drive_container->add_tile(diskii_button1, 0);
    drive_container->add_tile(diskii_button2, 1);
    drive_container->add_tile(unidisk_button1, 2);
    drive_container->add_tile(unidisk_button2, 3);

    // Initial layout
    drive_container->layout();

    // pop-up drive container when drives are spinning
    Container_t *dc2 = new Container_t(renderer, 10, HUD);  // Increased to 5 to accommodate the mouse position tile
    dc2->set_position(340, 800);
    dc2->set_size(420, 120);
    hud_diskii_1 = new DiskII_Button_t(aa, DiskII_Open, HUD); // this needs to have our disk key . or alternately use a different callback.
    hud_diskii_1->set_key(0x600);
    hud_diskii_1->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, 0x600});

    hud_diskii_2 = new DiskII_Button_t(aa, DiskII_Closed, HUD);
    hud_diskii_2->set_key(0x601);
    hud_diskii_2->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, 0x601});

    dc2->add_tile(hud_diskii_1, 0);
    dc2->add_tile(hud_diskii_2, 1);
    dc2->layout();
    hud_drive_container = dc2;

    // Create another container, this one for slots.
    Container_t *slot_container = new Container_t(renderer, 8, SC);  // Container for 8 slot buttons
    slot_container->set_position(100, 100);
    slot_container->set_size(320, 304);

    for (int i = 7; i >= 0; i--) {
        char slot_text[128];
        snprintf(slot_text, sizeof(slot_text), "Slot %d: %s", i, slot_manager->get_device(static_cast<SlotType_t>(i))->name);
        Button_t* slot = new Button_t(slot_text, SS);
        slot->set_text_renderer(text_render); // set text renderer for the button
        slot->set_size(300, 20);
        slot_container->add_tile(slot, 7 - i);    // Add in reverse order (7 to 0)
    }
    slot_container->layout();
    containers.push_back(slot_container);

    Container_t *mon_color_con = new Container_t(renderer, 5, SC);
    mon_color_con->set_position(100, 510);
    mon_color_con->set_size(320, 65);
    containers.push_back(mon_color_con);

    Style_t CB;
    CB.background_color = 0x00000000;
    CB.border_width = 1;
    CB.border_color = 0x000000FF;
    CB.padding = 2;
    Button_t *mc1 = new Button_t(aa, ColorDisplayButton, CB);
    Button_t *mc2 = new Button_t(aa, RGBDisplayButton, CB);
    Button_t *mc3 = new Button_t(aa, GreenDisplayButton, CB);
    Button_t *mc4 = new Button_t(aa, AmberDisplayButton, CB);
    Button_t *mc5 = new Button_t(aa, WhiteDisplayButton, CB);
    mc1->set_click_callback(set_color_display_ntsc, cpu);
    mc2->set_click_callback(set_color_display_rgb, cpu);
    mc3->set_click_callback(set_green_display, cpu);
    mc4->set_click_callback(set_amber_display, cpu);
    mc5->set_click_callback(set_white_display, cpu);
    mon_color_con->add_tile(mc1, 0);
    mon_color_con->add_tile(mc2, 1);
    mon_color_con->add_tile(mc3, 2);
    mon_color_con->add_tile(mc4, 3);
    mon_color_con->add_tile(mc5, 4);
    mon_color_con->layout();

    Container_t *speed_con = new Container_t(renderer, 4, SC);
    speed_con->set_position(100, 450);
    speed_con->set_size(260, 65);
    containers.push_back(speed_con);

    Button_t *sp1 = new Button_t(aa, MHz1_0Button, CB);
    Button_t *sp2 = new Button_t(aa, MHz2_8Button, CB);
    Button_t *sp3 = new Button_t(aa, MHz4_0Button, CB);
    Button_t *sp4 = new Button_t(aa, MHzInfinityButton, CB);
    sp1->set_click_callback(set_mhz_1_0, cpu);
    sp2->set_click_callback(set_mhz_2_8, cpu);
    sp3->set_click_callback(set_mhz_4_0, cpu);
    sp4->set_click_callback(set_mhz_infinity, cpu);
    speed_con->add_tile(sp1, 0);
    speed_con->add_tile(sp2, 1);
    speed_con->add_tile(sp3, 2);
    speed_con->add_tile(sp4, 3);
    speed_con->layout();
    speed_btn_10 = sp1;
    speed_btn_28 = sp2;
    speed_btn_40 = sp3;
    speed_btn_8 = sp4;

    Container_t *gen_con = new Container_t(renderer, 10, SC);
    gen_con->set_position(5, 100);
    gen_con->set_size(65, 300);
    Button_t *b1 = new Button_t(aa, ResetButton, CB);
    b1->set_click_callback(click_reset_cpu, cpu);
    gen_con->add_tile(b1, 0);
    gen_con->layout();
    containers.push_back(gen_con);

    Style_t ModalStyle;
    ModalStyle.background_color = 0xFFFFFFFF;
    ModalStyle.text_color = 0x000000FF;
    ModalStyle.border_width = 2;
    ModalStyle.border_color = 0xFF0000FF;
    ModalStyle.padding = 2;
    
    diskii_save_con = new ModalContainer_t(renderer, 10, "Disk Data has been modified. Save?", ModalStyle);
    diskii_save_con->set_position(300, 200);
    diskii_save_con->set_size(500, 200);
    // Create text buttons for the disk save dialog
    
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;
    
    save_btn = new Button_t("Save", TextButtonCfg);
    save_as_btn = new Button_t("Save As", TextButtonCfg);
    discard_btn = new Button_t("Discard", TextButtonCfg);
    cancel_btn = new Button_t("Cancel", TextButtonCfg);
    save_btn->set_size(100, 20);
    save_as_btn->set_size(100, 20);
    discard_btn->set_size(100, 20);
    cancel_btn->set_size(100, 20);
    save_btn->set_click_callback(modal_diskii_click, new diskii_modal_callback_data_t{this, diskii_save_con, 1});
    //save_as_btn->set_click_callback(modal_diskii_click, new diskii_modal_callback_data_t{this, diskii_save_con, 2});
    discard_btn->set_click_callback(modal_diskii_click, new diskii_modal_callback_data_t{this, diskii_save_con, 3});
    cancel_btn->set_click_callback(modal_diskii_click, new diskii_modal_callback_data_t{this, diskii_save_con, 4});
    diskii_save_con->add_tile(save_btn, 0);
    //diskii_save_con->add_tile(save_as_btn, 1);
    diskii_save_con->add_tile(discard_btn, 1);
    diskii_save_con->add_tile(cancel_btn, 2);
    diskii_save_con->layout();
    //containers.push_back(diskii_save_con); // just for testing
}

void OSD::update() {
    /** Control panel slide in/out logic */ 
    /* if control panel is sliding in, update position and acceleration */
    if (slideStatus == SLIDE_IN) {
        slidePosition+=slidePositionDelta;

        if (slidePosition > 0) {
            slidePosition = 0;
            slideStatus = SLIDE_NONE;
            currentSlideStatus = SLIDE_IN;
        }
        if (slidePositionDelta >= slidePositionDeltaMin) { // don't let it go to 0 or negative
            slidePositionDelta = slidePositionDelta - slidePositionAcceleration;
        }
    }

    /* if control panel is sliding out, update position and acceleration */
    if (slideStatus == SLIDE_OUT) {
        slidePosition-=slidePositionDelta;

        if (slidePosition < -slidePositionMax ) {
            slideStatus = SLIDE_NONE;
            currentSlideStatus = SLIDE_OUT;
            slidePosition = -slidePositionMax;
        }
        if (slidePositionDelta <= slidePositionDeltaMax) {
            slidePositionDelta = slidePositionDelta + slidePositionAcceleration;
        }
    }

    static int updCount=0;
    if (updCount++ > 60) {
        updCount = 0;
        cpu->mounts->dump();
    }

    // TODO: iterate over all drives based on what's in slots.
    // update disk status
    diskii_button1->set_disk_status(cpu->mounts->media_status(0x600));
    diskii_button2->set_disk_status(cpu->mounts->media_status(0x601));
    hud_diskii_1->set_disk_status(cpu->mounts->media_status(0x600));
    hud_diskii_2->set_disk_status(cpu->mounts->media_status(0x601));
    unidisk_button1->set_disk_status(cpu->mounts->media_status(0x500));
    unidisk_button2->set_disk_status(cpu->mounts->media_status(0x501));

    // background color update based on clock speed to highlight current button.
    speed_btn_10->set_background_color(0x000000FF);
    speed_btn_28->set_background_color(0x000000FF);
    speed_btn_40->set_background_color(0x000000FF);
    speed_btn_8->set_background_color(0x000000FF);
    if (cpu->clock_mode == CLOCK_1_024MHZ) {
        speed_btn_10->set_background_color(0x00FF00FF);
    } else if (cpu->clock_mode == CLOCK_2_8MHZ) {
        speed_btn_28->set_background_color(0x00FF00FF);
    } else if (cpu->clock_mode == CLOCK_4MHZ) {
        speed_btn_40->set_background_color(0x00FF00FF);
    } else if (cpu->clock_mode == CLOCK_FREE_RUN) {
        speed_btn_8->set_background_color(0x00FF00FF);
    }

    if (activeModal) {
        activeModal->render();
    }

}

/** Draw the control panel (if visible) */
void OSD::render() {

    /** if current Status is out, don't draw. If status is in transition or IN, draw. */
    if (currentSlideStatus == SLIDE_IN || (currentSlideStatus != slideStatus)) {
        float ox,oy;
        SDL_GetRenderScale(renderer, &ox, &oy);
        SDL_SetRenderScale(renderer, 1,1); // TODO: calculate these based on window size

        /* ----- */
        /* Redraw the whole control panel from bottom up, because the modal could have been anywhere! */
        SDL_SetRenderTarget(renderer, cpTexture);
        
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // make the background opaque and black.
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
        SDL_RenderFillRect(renderer, NULL);

        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xC0);
        SDL_FRect rect = {0, 50, (float)(window_w-100), (float)(window_h-100)};
        SDL_RenderFillRect(renderer, &rect);
        
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0xFF, 0xFF);
        SDL_RenderDebugText(renderer, 50, 80, "This is your menu. It isn't very done, hai!");

        /* ----- */

        SDL_FRect cpTargetRect = {
            (float)slidePosition,
            (float)0, // no vertical offset
            (float)window_w+slidePosition, 
            (float)window_h
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        //printFRect(&cpTargetRect);
        
        // Render the container and its buttons into the cpTexture
        SDL_SetRenderTarget(renderer, cpTexture);
        for (Container_t* container : containers) {
            container->render();
        }
        if (activeModal) {
            activeModal->render();
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);

        SDL_SetRenderTarget(renderer, nullptr);

        // now render the cpTexture into window
        SDL_RenderTexture(renderer, cpTexture, NULL, &cpTargetRect);
        SDL_SetRenderScale(renderer, ox,oy);
    } 
    if (currentSlideStatus == SLIDE_OUT) {
        drive_status_t ds1 = diskii_button1->get_disk_status();
        drive_status_t ds2 = diskii_button2->get_disk_status();

        // Get the current window size to properly position HUD elements
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        float ox,oy;
        SDL_GetRenderScale(renderer, &ox, &oy);
        SDL_SetRenderScale(renderer, 1,1); // TODO: calculate these based on window size

        if (ds1.motor_on || ds2.motor_on) {

            // Update HUD drive container position based on window size
            // Position it at the bottom of the screen with some padding
            hud_drive_container->set_position((window_width - 420) / 2, window_height - 125 );

            // display running disk drives at the bottom of the screen.
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            hud_drive_container->render();
        }

        // display the MHz at the bottom of the screen.
        { // we are currently at A2 display scale.
            char hud_str[150];
            snprintf(hud_str, sizeof(hud_str), "MHz: %8.4f", cpu->e_mhz);
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderDebugText(renderer, 20, window_height - 30, hud_str);
#if 0
            snprintf(hud_str, sizeof(hud_str), "Cycles          PC   A  X  Y  P  (N V B D I Z C)");
            SDL_RenderDebugText(renderer, 20, window_height - 50, hud_str);
            snprintf(hud_str, sizeof(hud_str), "%-15lld %04X %02X %02X %02X %02X (%1d %1d %1d %1d %1d %1d %1d)", cpu->cycles, cpu->pc, cpu->a, cpu->x, cpu->y, cpu->p, cpu->N, cpu->V, cpu->B, cpu->D, cpu->I, cpu->Z, cpu->C);
            SDL_RenderDebugText(renderer, 20, window_height - 40, hud_str);
#endif
        }
        SDL_SetRenderScale(renderer, ox,oy);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    }
}

bool OSD::event(const SDL_Event &event) {
    bool active = (currentSlideStatus == SLIDE_IN);
    if (active) {
        if (activeModal) {
            activeModal->handle_mouse_event(event);
        } else {
            // Let containers have a stab at the event
            for (Container_t* container : containers) {
                container->handle_mouse_event(event);
            }
        }
    }

    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
            //printf("osd key down: %d %d %d\n", event.key.key, slideStatus, currentSlideStatus);
            if (event.key.key == SDLK_F4) {
                SDL_SetWindowRelativeMouseMode(window, false);
                // if we're already in motion, disregard this for now.
                if (!slideStatus) {
                    if (currentSlideStatus == SLIDE_IN) { // we are in right now, slide it out
                        slideStatus = SLIDE_OUT;   
                        slidePosition = 0;
                        slidePositionDelta = slidePositionDeltaMin;
                    } else if (currentSlideStatus == SLIDE_OUT) {
                        slideStatus = SLIDE_IN; // slide it in right to the top
                        slidePosition = -slidePositionMax;
                        slidePositionDelta = slidePositionDeltaMax;
                    }
                }
                return(true);
            }
            break;
    }    
    return(active);
}

void OSD::show_diskii_modal(uint64_t key, uint64_t data) {
    activeModal = diskii_save_con;
    diskii_save_con->set_key(key);
    diskii_save_con->set_data(data);
}

void OSD::close_diskii_modal(uint64_t key, uint64_t data) {
    activeModal = nullptr;
}
