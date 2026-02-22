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

#include <stdexcept>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>

#include "Device_ID.hpp"
#include "cpu.hpp"
#include "computer.hpp"
#include "DiskII_Button.hpp"
#include "Unidisk_Button.hpp"
#include "AppleDisk_525_Button.hpp"
#include "Container.hpp"
#include "AssetAtlas.hpp"
#include "Style.hpp"
#include "MainAtlas.hpp"
#include "OSD.hpp"
#include "display/display.hpp"
#include "util/mount.hpp"
#include "util/SoundEffect.hpp"
#include "util/SoundEffectKeys.hpp"
#include "util/strndup.h"
#include "ModalContainer.hpp"
#include "util/printf_helper.hpp"
#include "paths.hpp"


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
    
    // Remember the full file path for next time - SDL3 uses it to determine initial directory
    std::string filepath(filelist[0]);
    Paths::set_last_file_dialog_dir(filepath);
    
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
    osd->computer->mounts->mount_media(dm);
}

void diskii_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    OSD *osd = data->osd;

    if (osd->computer->mounts->media_status(data->key).is_mounted) {
        // if media was modified, create Event to handle modal dialog. Otherwise, just unmount.
        if (osd->computer->mounts->media_status(data->key).is_modified) {
            osd->show_diskii_modal(data->key, 0);
        } else {
            osd->computer->mounts->unmount_media(data->key, DISCARD);
        }
        return;
    }

    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "do;po;woz;dsk;hdv;2mg" },
        { "All files",   "*" }
    };

    printf("diskii button clicked\n");
    const std::string& last_path = Paths::get_last_file_dialog_dir();
    SDL_ShowOpenFileDialog(file_dialog_callback, 
        userdata, 
        osd->get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        last_path.c_str(),
        false);
}

void unidisk_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    OSD *osd = data->osd;

    if (osd->computer->mounts->media_status(data->key).is_mounted) {
        disk_mount_t dm;
        osd->computer->mounts->unmount_media(data->key, DISCARD); // TODO: we write blocks as we go, there is nothing to 'save' here.
        return;
    }
    
    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "po;dsk;hdv;2mg" },
        { "All files",   "*" }
    };

    printf("unidisk button clicked\n");
    const std::string& last_path = Paths::get_last_file_dialog_dir();
    SDL_ShowOpenFileDialog(file_dialog_callback, 
        userdata, 
        osd->get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        last_path.c_str(),
        false);
}

void set_color_display_ntsc(void *data) {
    printf("set_color_display_ntsc %p\n", data);
    display_state_t *ds = (display_state_t *)data;
    ds->video_system->set_display_engine(DM_ENGINE_NTSC);
}

void set_color_display_rgb(void *data) {
    printf("set_color_display_rgb %p\n", data);
    display_state_t *ds = (display_state_t *)data;
    ds->video_system->set_display_engine(DM_ENGINE_RGB);
}

void set_green_display(void *data) {
    printf("set_green_display %p\n", data);
    display_state_t *ds = (display_state_t *)data;
    ds->video_system->set_display_mono_color(DM_MONO_GREEN);
    ds->video_system->set_display_engine(DM_ENGINE_MONO);
}

void set_amber_display(void *data) {
    printf("set_amber_display %p\n", data);
    display_state_t *ds = (display_state_t *)data;
    ds->video_system->set_display_mono_color(DM_MONO_AMBER);
    ds->video_system->set_display_engine(DM_ENGINE_MONO);
}

void set_white_display(void *data) {
    printf("set_white_display %p\n", data);
    display_state_t *ds = (display_state_t *)data;
    ds->video_system->set_display_mono_color(DM_MONO_WHITE);
    ds->video_system->set_display_engine(DM_ENGINE_MONO);
}

#if 0
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

void set_mhz_7_1(void *data) {
    printf("set_mhz_7_1 %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_7_159MHZ);
}

void set_mhz_14_3(void *data) {
    printf("set_mhz_14_3 %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_7_159MHZ);
}

void set_mhz_infinity(void *data) {
    printf("set_mhz_infinity %p\n", data);
    cpu_state *cpu = (cpu_state *)data;
    set_clock_mode(cpu, CLOCK_FREE_RUN);
}
#endif

void click_reset_cpu(void *data) {
    printf("click_reset_cpu %p\n", data);
    // TODO: fix this. OSD should tell computer() to reset. Or, pass an event to main loop.
    computer_t *computer = (computer_t *)data;
    computer->reset(false);
}

void close_btn_click(void *data) {
    printf("close_btn_click %p\n", data);
    OSD *osd = (OSD *)data;
    osd->close_panel();
}

void open_btn_click(void *data) {
    printf("open_btn_click %p\n", data);
    OSD *osd = (OSD *)data;
    osd->open_panel();
}

void modal_diskii_click(void *data) {
    diskii_modal_callback_data_t *d = (diskii_modal_callback_data_t *)data;
    printf("modal_diskii_click %p %llu\n", data, u64_t(d->key));
    OSD *osd = d->osd;
    cpu_state *cpu = osd->cpu;
    ModalContainer_t *container = d->container;
    osd->event_queue->addEvent(new Event(EVENT_MODAL_CLICK, container->get_key(), d->key));
    // I need to reference back to the button that was clicked and get its ID.
}


/** -------------------------------------------------------------------------------------------------- */

SDL_Window* OSD::get_window() { 
    return window; 
}

void OSD::set_raise_window() {
    event_queue->addEvent(new Event(EVENT_REFOCUS, 0, (uint64_t)0));
}

OSD::OSD(computer_t *computer, cpu_state *cpu, SDL_Renderer *rendererp, SDL_Window *windowp, SlotManager_t *slot_manager, int window_width, int window_height, AssetAtlas_t *aa) 
    : renderer(rendererp), window(windowp), window_w(window_width), window_h(window_height), computer(computer), cpu(cpu), slot_manager(slot_manager), aa(aa) {

    event_queue = computer->event_queue;

    cpTexture = SDL_CreateTexture(renderer,
        PIXEL_FORMAT,
        SDL_TEXTUREACCESS_TARGET,
        window_w, 
        window_h
    );

    if (!cpTexture) {
        throw std::runtime_error(std::string("Error creating cpTexture: ") + SDL_GetError());
    }

    /* Setup a text renderer for this OSD */
    text_render = new TextRenderer(rendererp, "fonts/OpenSans-Regular.ttf", 15.0f);
    title_trender = new TextRenderer(rendererp, "fonts/OpenSans-Regular.ttf", 30.0f);
    text_render->set_color(0xFF, 0xFF, 0xFF, 0xFF);
    title_trender->set_color(0, 0, 0, 0xFF);

    Style_t CS;
    CS.padding = 4;
    CS.border_width = 2;
    CS.background_color = 0xFFFFFFFF;
    CS.border_color = 0x008000E0;
    CS.hover_color = 0x008080FF;
    Style_t DC;
    DC.background_color = 0x00000040;
    DC.border_color = 0xFFFFFF80;
    DC.hover_color = 0x008080FF;
    DC.padding = 4;
    DC.border_width = 2;
    Style_t DS;
    DS.background_color = 0x00000000;
    DS.border_color = 0x00000000;
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

    // Create a container for our drive buttons
    drive_container = new Container_t(renderer, 10, DC);
    drive_container->set_position(600, 70);
    drive_container->set_tile_size(415, 600);
    containers.push_back(drive_container);

    // TODO: create buttons based on what is in slots.

#if 0  // Old slot-scanning code - replaced by Mounts interface
    int diskii_slot = -1;
    int unidisk_slot = -1;
    for (int i = 0; i < NUM_SLOTS; i++) {
        Device_t *device = slot_manager->get_device(static_cast<SlotType_t>(i));
        if (device->id == DEVICE_ID_DISK_II) {
            diskii_slot = i;
        } else if (device->id == DEVICE_ID_PD_BLOCK2) {
            unidisk_slot = i;
        }
    }

    // Create the buttons
    int tile_id = 0;
    if (diskii_slot != -1) {
        uint64_t key = (uint64_t)diskii_slot << 8 | 0;
        diskii_button1 = new DiskII_Button_t(aa, DiskII_Open, DS); // this needs to have our disk key . or alternately use a different callback.
        diskii_button1->set_key(key);
        diskii_button1->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, key});

        diskii_button2 = new DiskII_Button_t(aa, DiskII_Closed, DS);
        diskii_button2->set_key(key | 1);
        diskii_button2->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, key | 1});

        drive_container->add_tile(diskii_button1, tile_id++);
        drive_container->add_tile(diskii_button2, tile_id++);  
        
        // pop-up drive container when drives are spinning
        Container_t *dc2 = new Container_t(renderer, 10, HUD);  // Increased to 5 to accommodate the mouse position tile
        dc2->set_position(340, 800);
        dc2->set_tile_size(420, 120);
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
    }
    if (unidisk_slot != -1) {
        uint64_t key = (uint64_t)unidisk_slot << 8 | 0;
        unidisk_button1 = new Unidisk_Button_t(aa, Unidisk_Face, DS); // this needs to have our disk key . or alternately use a different callback.
        unidisk_button1->set_key(key);
        unidisk_button1->set_click_callback(unidisk_button_click, new diskii_callback_data_t{this, key | 0});

        unidisk_button2 = new Unidisk_Button_t(aa, Unidisk_Face, DS); // this needs to have our disk key . or alternately use a different callback.
        unidisk_button2->set_key(key | 1);
        unidisk_button2->set_click_callback(unidisk_button_click, new diskii_callback_data_t{this, key | 1});

        drive_container->add_tile(unidisk_button1, tile_id++);
        drive_container->add_tile(unidisk_button2, tile_id++);
    }
#endif

    // New Mounts-based button creation
    // Get all registered drives from the Mounts system
    const std::vector<drive_info_t>& drives = computer->mounts->get_all_drives();
    
    int tile_id = 0;
    bool has_hud_drives = false;
    
    // Create buttons for each registered drive
    for (const auto& drive : drives) {
        uint8_t slot = drive.key >> 8;
        uint8_t drive_num = drive.key & 0xFF;
        StorageButton *button;

        // Create the appropriate button type based on drive_type
        if (drive.drive_type == DRIVE_TYPE_DISKII) {
            button = new DiskII_Button_t(aa, DiskII_Open, DS);
            button->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, drive.key});
        } else if (drive.drive_type == DRIVE_TYPE_APPLEDISK_525) {
            button = new AppleDisk_525_Button_t(aa, AppleDisk_525_Open, DS);
            button->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, drive.key});
        } else if (drive.drive_type == DRIVE_TYPE_PRODOS_BLOCK) {
            button = new Unidisk_Button_t(aa, Unidisk_Face, DS);
            button->set_click_callback(unidisk_button_click, new diskii_callback_data_t{this, drive.key});
        }
        button->set_key(drive.key);
        button->set_click_callback(diskii_button_click, new diskii_callback_data_t{this, drive.key});
        drive_container->add_tile(button, tile_id++);
    }
    // Initial layout for drive container
    drive_container->layout();

    /*
     instead of creating whole new buttons, we insert the same buttons into this container.
     this needs to be dynamic, based on which slot is active at any given time.
     Create HUD drive container for first DiskII slot found 
    */
    hud_drive_container = new Container_t(renderer, 10, HUD);
    hud_drive_container->set_position(340, 800);
    hud_drive_container->set_tile_size(420, 120);
    hud_drive_container->layout();

    // Create another container, this one for slots.
    Container_t *slot_container = new Container_t(renderer, 8, SC);  // Container for 8 slot buttons
    slot_container->set_position(100, 100);
    slot_container->set_tile_size(320, 304);

    for (int i = 7; i >= 0; i--) {
        char slot_text[128];
        snprintf(slot_text, sizeof(slot_text), "Slot %d: %s", i, slot_manager->get_device(static_cast<SlotType_t>(i))->name);
        Button_t* slot = new Button_t(slot_text, text_render, SS);
        //slot->set_text_renderer(text_render); // set text renderer for the button
        slot->set_tile_size(300, 30);
        slot_container->add_tile(slot, 7 - i);    // Add in reverse order (7 to 0)
    }
    slot_container->layout();
    containers.push_back(slot_container);

    Container_t *mon_color_con = new Container_t(renderer, 5, SC);
    mon_color_con->set_position(100, 510);
    mon_color_con->set_tile_size(320, 65);
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
    display_state_t *ds = (display_state_t *)get_module_state(cpu, MODULE_DISPLAY);
    mc1->set_click_callback(set_color_display_ntsc, ds);
    mc2->set_click_callback(set_color_display_rgb, ds);
    mc3->set_click_callback(set_green_display, ds);
    mc4->set_click_callback(set_amber_display, ds);
    mc5->set_click_callback(set_white_display, ds);
    mon_color_con->add_tile(mc1, 0);
    mon_color_con->add_tile(mc2, 1);
    mon_color_con->add_tile(mc3, 2);
    mon_color_con->add_tile(mc4, 3);
    mon_color_con->add_tile(mc5, 4);
    mon_color_con->layout();

    Container_t *speed_con = new Container_t(renderer, 6, SC);
    speed_con->set_position(100, 450);
    speed_con->set_tile_size(320, 65);
    containers.push_back(speed_con);

    speed_btn_10 = new Button_t(aa, MHz1_0Button, CB);
    speed_btn_28 = new Button_t(aa, MHz2_8Button, CB);
    speed_btn_71 = new Button_t(aa, MHz7_159Button, CB);
    speed_btn_14 = new Button_t(aa, MHz14_318Button, CB);
    speed_btn_8 = new Button_t(aa, MHzInfinityButton, CB);
    
    speed_btn_10->set_click_callback([this](const SDL_Event& event) -> bool {
        this->clock->set_clock_mode(CLOCK_1_024MHZ);
        return true;
    });
    speed_btn_28->set_click_callback([this](const SDL_Event& event) -> bool {
        this->clock->set_clock_mode(CLOCK_2_8MHZ);
        return true;
    });
    speed_btn_71->set_click_callback([this](const SDL_Event& event) -> bool {
        this->clock->set_clock_mode(CLOCK_7_159MHZ);
        return true;
    });
    speed_btn_14->set_click_callback([this](const SDL_Event& event) -> bool {
        this->clock->set_clock_mode(CLOCK_14_3MHZ);
        return true;
    });
    speed_btn_8->set_click_callback([this](const SDL_Event& event) -> bool {
        this->clock->set_clock_mode(CLOCK_FREE_RUN);
        return true;
    });
    speed_con->add_tile(speed_btn_10, 0);
    speed_con->add_tile(speed_btn_28, 1);
    speed_con->add_tile(speed_btn_71, 2);
    speed_con->add_tile(speed_btn_14, 3);
    speed_con->add_tile(speed_btn_8, 4);
    
    speed_con->layout();
    /* speed_btn_10 = sp1;
    speed_btn_28 = sp2;
    speed_btn_71 = sp3;
    speed_btn_8 = sp4;
    speed_btn_14 = sp5; */

    Container_t *gen_con = new Container_t(renderer, 10, SC);
    gen_con->set_position(5, 100);
    gen_con->set_tile_size(65, 300);
    Button_t *b1 = new Button_t(aa, ResetButton, CB);
    b1->set_click_callback([this,computer](const SDL_Event& event) -> bool {
        computer->reset(false);
        return true;
    });
    gen_con->add_tile(b1, 0);
    gen_con->layout();
    containers.push_back(gen_con);

    Style_t ModalStyle;
    ModalStyle.background_color = 0xFFFFFFFF;
    ModalStyle.text_color = 0x000000FF;
    ModalStyle.border_width = 2;
    ModalStyle.border_color = 0xFF0000FF;
    ModalStyle.padding = 2;
    
    diskii_save_con = new ModalContainer_t(renderer, text_render, 10, "Disk Data has been modified. Save?", ModalStyle);
    diskii_save_con->set_position(300, 200);
    diskii_save_con->set_tile_size(500, 200);
    // Create text buttons for the disk save dialog
    
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;
    
    save_btn = new Button_t("Save", text_render, TextButtonCfg);
    save_as_btn = new Button_t("Save As", text_render, TextButtonCfg);
    discard_btn = new Button_t("Discard", text_render, TextButtonCfg);
    cancel_btn = new Button_t("Cancel", text_render, TextButtonCfg);
    save_btn->set_tile_size(100, 30);
    save_as_btn->set_tile_size(100, 30);
    discard_btn->set_tile_size(100, 30);
    cancel_btn->set_tile_size(100, 30);
    //save_btn->set_text_renderer(text_render);
    ////save_as_btn->set_text_renderer(text_render);
    //discard_btn->set_text_renderer(text_render);
    //cancel_btn->set_text_renderer(text_render);
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
    
    close_btn = new Button_t("<", TextButtonCfg);
    close_btn->set_click_callback(close_btn_click, this);
    close_btn->set_tile_size(36, 36);
    close_btn->set_tile_position(window_w-100, 49);

    open_btn = new FadeButton_t(">", TextButtonCfg);
    open_btn->set_click_callback(open_btn_click, this);
    open_btn->set_tile_size(36, 36);
    open_btn->set_tile_position(0, 50);
    open_btn->set_fade_frames(512, 4); // hold for one second, then fade out over next second. (roughly)

}

OSD::~OSD() {
    SDL_DestroyTexture(cpTexture);
    delete text_render;
    delete title_trender;
    for (Container_t* container : containers) {
        delete container;
    }
    delete diskii_save_con;
    delete close_btn;
    delete open_btn;
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
        // computer->mounts->dump(); // TODO: this is crashing for unknown reason.
    }

    // update disk status - iterate over all drives based on what's in slots
    uint64_t key_mask = 0;

    // two pass. First, update buttons and calculate the key mask. (the lit drive could have been the previous one, hence 2-pass.)
    for (int i = 0; i < drive_container->get_tile_count(); i++) {
        Tile_t *tile = drive_container->get_tile(i);
        if (tile) {
            StorageButton *button = dynamic_cast<StorageButton *>(tile);
            uint64_t key = button->get_key();
            drive_status_t ds = computer->mounts->media_status(key);
            button->set_disk_status(ds);
            if (ds.motor_on) {
                key_mask |= key & 0xFFFFFF00;
            }            
        }
    }

    // update the HUD container.
    hud_drive_container->remove_all_tiles(); // always clear.. 
    if ((currentSlideStatus == SLIDE_OUT)  && (key_mask)) {
        // second pass, update the hud container with items matching the key mask.
        // and set their hover status to false.
        uint32_t hud_index = 0;
        for (int i = 0; i < drive_container->get_tile_count(); i++) {
            Tile_t *tile = drive_container->get_tile(i);
            if (tile) {
                StorageButton *button = dynamic_cast<StorageButton *>(tile);
                uint64_t key = button->get_key();
                drive_status_t ds = button->get_disk_status();
                if ((key & 0xFFFFFF00) == key_mask) {
                    hud_drive_container->add_tile(button, hud_index++);
                    button->on_hover_changed(false);
                }
            }            
        }
    }

    // background color update based on clock speed to highlight current button.
    speed_btn_10->set_background_color(0x000000FF);
    speed_btn_28->set_background_color(0x000000FF);
    speed_btn_71->set_background_color(0x000000FF);
    speed_btn_8->set_background_color(0x000000FF);
    speed_btn_14->set_background_color(0x000000FF);
    switch (this->clock->get_clock_mode()) {
        case CLOCK_1_024MHZ:
            speed_btn_10->set_background_color(0x00FF00FF);
            break;
        case CLOCK_2_8MHZ:
            speed_btn_28->set_background_color(0x00FF00FF);
            break;
        case CLOCK_7_159MHZ:
            speed_btn_71->set_background_color(0x00FF00FF);
            break;
        case CLOCK_14_3MHZ:
            speed_btn_14->set_background_color(0x00FF00FF);
            break;
        case CLOCK_FREE_RUN:
            speed_btn_8->set_background_color(0x00FF00FF);
            break;
        default:
            break; // should never happen..
    }

    if (activeModal) {
        activeModal->render();
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF); // TODO: a dirty hack to make sure the background is black.
}

void OSD::set_heads_up_message(const std::string &text, int count) {
    headsUpMessageText = text;
    headsUpMessageCount = count;
}

/** Draw the control panel (if visible) */
void OSD::render() {

    /** if current Status is out, don't draw. If status is in transition or IN, draw. */
    if (currentSlideStatus == SLIDE_IN || (slideStatus && (currentSlideStatus != slideStatus))) {
        float ox,oy;
        SDL_GetRenderScale(renderer, &ox, &oy);
        SDL_SetRenderScale(renderer, 1.0,1.0); // TODO: calculate these based on window size

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
      
        /* ----- */

        SDL_FRect cpTargetRect = {
            (float)slidePosition,
            (float)0, // no vertical offset
            (float)window_w+slidePosition, 
            (float)window_h
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        //SDL_RenderDebugText(renderer, 50, 80, "This is your menu. It isn't very done, hai!");
        title_trender->render("Control Panel", 100, 50);

        close_btn->render(renderer);

        // re-lay this out since the hud might have changed the buttons.
        drive_container->layout();

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


        // Get the current window size to properly position HUD elements
        int window_width, window_height;
        SDL_GetWindowSize(window, &window_width, &window_height);
        float ox,oy;
        SDL_GetRenderScale(renderer, &ox, &oy);
        SDL_SetRenderScale(renderer, 1,1); // TODO: calculate these based on window size

        if (headsUpMessageCount) { // set it to 512 for instance to sit at full opacity for 4 seconds then fade out over 4ish seconds.
            int opacity = headsUpMessageCount < 255 ? headsUpMessageCount : 255;
            //SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, opacity);
            text_render->set_color(0xFF, 0xFF, 0xFF, opacity);
            text_render->render(headsUpMessageText, window_width/2, 30, TEXT_ALIGN_CENTER);
            
            headsUpMessageCount -= 3;
            if (headsUpMessageCount < 0) headsUpMessageCount = 0;
        }

        open_btn->render(renderer); // this now takes care of its own fade-out.

        if (hud_drive_container->get_tile_count() > 0) {
            hud_drive_container->layout();
            hud_drive_container->set_position(((float)window_width - 420) / 2, window_height - 125 );

            // display running disk drives at the bottom of the screen.
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            hud_drive_container->render();
        }
        
/*         if (diskii_button1) {
            drive_status_t ds1 = diskii_button1->get_disk_status();
            drive_status_t ds2 = diskii_button2->get_disk_status();

            if (ds1.motor_on || ds2.motor_on) {

                // Update HUD drive container position based on window size
                // Position it at the bottom of the screen with some padding
                hud_drive_container->set_position(((float)window_width - 420) / 2, window_height - 125 );

                // display running disk drives at the bottom of the screen.
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

                hud_drive_container->render();
            }
        }
 */        
        // display the MHz at the bottom of the screen.
        { // we are currently at A2 display scale.
            char hud_str[150];
            snprintf(hud_str, sizeof(hud_str), "MHz: %8.4f / FPS %8.4f / Idle: %5.1f%%", cpu->e_mhz, cpu->fps, cpu->idle_percent);
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderDebugText(renderer, 20, window_height - 30, hud_str);

            if (clock->get_video_scanner()) {
                snprintf(hud_str, sizeof(hud_str), "H: %3d V: %3d c: %6d", clock->get_video_scanner()->hcount, clock->get_video_scanner()->get_vcount(), clock->get_video_scanner()->get_frame_scan()->get_count());
                SDL_RenderDebugText(renderer, 20, window_height - 50, hud_str);
            }
            
            uint64_t etime, esecs, emsecs;
        }
        SDL_SetRenderScale(renderer, ox,oy);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    }
}

bool OSD::event(const SDL_Event &event) {
    //if mouse is captured we ignore events here.
    if (SDL_GetWindowRelativeMouseMode(window)) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            return(false);
        }
    }

    bool active = (currentSlideStatus == SLIDE_IN);
    if (active) {
        if (activeModal) {
            activeModal->handle_mouse_event(event);
        } else {
            close_btn->handle_mouse_event(event);
            // Let containers have a stab at the event
            for (Container_t* container : containers) {
                container->handle_mouse_event(event);
            }
        }
    } else {
        if (open_btn->handle_mouse_event(event)) {
            return(true);
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
                        close_panel();
                    } else if (currentSlideStatus == SLIDE_OUT) {
                        open_panel();
                    }
                }
                return(true);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (!SDL_GetWindowRelativeMouseMode(window)) {
                open_btn->reset_fade();
            }
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            return(false); // we don't handle this, have gs2 loop send it to video_system.
            break;
    }    
    return(active);
}

void OSD::open_panel() {
    slideStatus = SLIDE_IN; // slide it in right to the top
    slidePosition = -slidePositionMax;
    slidePositionDelta = slidePositionDeltaMax;
}

void OSD::close_panel() {
    slideStatus = SLIDE_OUT;   
    slidePosition = 0;
    slidePositionDelta = slidePositionDeltaMin;
}

void OSD::show_diskii_modal(uint64_t key, uint64_t data) {
    activeModal = diskii_save_con;
    diskii_save_con->set_key(key);
    diskii_save_con->set_data(data);
}

void OSD::close_diskii_modal(uint64_t key, uint64_t data) {
    activeModal = nullptr;
}
