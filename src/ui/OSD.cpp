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

#include "SDL3/SDL_events.h"
#include "computer.hpp"
#include "LabeledButton.hpp"
#include "Container.hpp"
#include "AssetAtlas.hpp"
#include "Style.hpp"
#include "MainAtlas.hpp"
#include "OSD.hpp"
#include "display/display.hpp"
#include "util/StorageDevice.hpp"
#include "util/mount.hpp"
#include "util/strndup.h"
#include "ModalContainer.hpp"
#include "UIContext.hpp"
#include "util/printf_helper.hpp"
#include "paths.hpp"
#include "util/MenuInterface.h"
#include "util/SystemConfig.hpp"
#include "platform-specific/menu.h"
#if defined(__EMSCRIPTEN__)
#include "platform-specific/emscripten/web_file_dialog.hpp"
#endif
#include "systemconfig.hpp"
#include "SelectButton.hpp"
#include "DrivesHUD.hpp"
#include "SpeedSelect.hpp"
#include "HoverControls.hpp"
#include "DirtyDiskSave.hpp"
#include "QuitModal.hpp"
#include "ConfigSelectors.hpp"
#include "StorageButtonFactory.hpp"

// we need to use data passed to us, and pass it to the ShowOpenFileDialog, so when the file select event
// comes back later, we know which drive this was for.
// TODO: only allow one of these to be open at a time. If one is already open, disregard.

struct diskii_callback_data_t {
    OSD *osd;
    storage_key_t key;
};

struct diskii_modal_callback_data_t {
    OSD *osd;
    ModalContainer_t *container;
    storage_key_t key;
};

/** handle file dialog callback — userdata is owned by the caller, not freed here */
static void /* SDLCALL */ file_dialog_callback(void* userdata, const char* const* filelist, int filter)
{
     diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;

    OSD *osd = data->osd;
    osd->set_raise_window();

    // SDL: NULL = error; non-NULL with filelist[0] == NULL = cancel (portal uses { NULL })
    if (!filelist || !filelist[0]) return;

    // returns callback: /Users/bazyar/src/AppleIIDisks/33master.dsk when selecting
    // a disk image file.
    printf("file_dialog_callback: %s\n", filelist[0]);

    Paths::set_last_file_dialog_dir(filelist[0]);
    
    // 1. unmount current image (if present).
    // 2. mount new image.

    disk_mount_t dm;
    dm.filename = strndup(filelist[0], 1024);
    dm.slot = data->key.slot;
    dm.drive = data->key.drive;   
    bool result = osd->computer->mounts->mount_media(dm);
    if (!result) {
        osd->set_heads_up_message("Failed to mount media", 512);
    }

}

/** Wrapper used when userdata was heap-allocated: delegates then frees. */
static void menu_file_dialog_callback(void* userdata, const char* const* filelist, int filter) {
    file_dialog_callback(userdata, filelist, filter);
    delete (diskii_callback_data_t*)userdata;
}

void OSD::open_file_dialog(storage_key_t key) {
    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "do;po;woz;dsk;hdv;2mg;img" },
        //{ "Partition Maps", "pmap" }, // this doesn't go here.
        { "All files",   "*" }
    };

    diskii_callback_data_t *data = new diskii_callback_data_t();
    data->osd = this;
    data->key = key;

#if defined(__EMSCRIPTEN__)
    web_open_file_dialog(menu_file_dialog_callback, data,
        ".do,.po,.woz,.dsk,.hdv,.2mg,.img");
#else
    const std::string& last_path = Paths::get_last_file_dialog_dir();
    SDL_ShowOpenFileDialog(menu_file_dialog_callback,
        data,
        get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        last_path.empty() ? nullptr : last_path.c_str(),
        false);
#endif
}

void handle_disk_toggle(computer_t *computer, OSD *osd, storage_key_t key) {
    auto status = computer->mounts->media_status(key);
    if (status.is_mounted) {
        if (status.is_modified) {
            osd->show_diskii_modal(key, 0);
        } else {
            computer->mounts->unmount_media(key, DISCARD);
        }
        return;
    }
    osd->open_file_dialog(key);
}

void diskii_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    handle_disk_toggle(data->osd->computer, data->osd, data->key);
}

// TODO: not unidisk, actually BazFast
void bazfast_button_click(void *userdata) {
    diskii_callback_data_t *data = (diskii_callback_data_t *)userdata;
    OSD *osd = data->osd;

    if (osd->computer->mounts->media_status(data->key).is_mounted) {
        disk_mount_t dm;
        osd->computer->mounts->unmount_media(data->key, DISCARD); // TODO: we write blocks as we go, there is nothing to 'save' here.
        return;
    }
    
    static const SDL_DialogFileFilter filters[] = {
        { "Disk Images",  "po;dsk;hdv;2mg;img;pmap" },
        { "All files",   "*" }
    };

    printf("unidisk button clicked\n");
#if defined(__EMSCRIPTEN__)
    web_open_file_dialog(file_dialog_callback, userdata,
        ".po,.dsk,.hdv,.2mg,.img,.pmap");
#else
    const std::string& last_path = Paths::get_last_file_dialog_dir();
    SDL_ShowOpenFileDialog(file_dialog_callback, 
        userdata, 
        osd->get_window(),
        filters,
        sizeof(filters)/sizeof(SDL_DialogFileFilter),
        last_path.empty() ? nullptr : last_path.c_str(),
        false);
#endif
}

/** -------------------------------------------------------------------------------------------------- */

SDL_Window* OSD::get_window() { 
    return window; 
}

void OSD::set_raise_window() {
    event_queue->addEvent(new Event(EVENT_REFOCUS, 0, (uint64_t)0));
}

OSD::OSD(computer_t *computer, SDL_Renderer *rendererp, SDL_Window *windowp, SlotManager_t *slot_manager, int window_width, int window_height, AssetAtlas_t *aa) 
    : renderer(rendererp), window(windowp), window_w(window_width), window_h(window_height), computer(computer), slot_manager(slot_manager), aa(aa),
    clock(computer->clock) {

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

    ui_ctx = { renderer, windowp, text_render, title_trender, aa };

    Style_t CS = {
        .background_color = 0xFFFFFFFF,
        .border_color = 0x008000E0,
        .hover_color = 0x008080FF,
        .padding = 4,
        .border_width = 2,
    };
    Style_t DC = {
        .background_color = 0x00000040,
        .border_color = 0xFFFFFF80,
        .hover_color = 0x008080FF,
        .padding = 4,
        .border_width = 2,
    };
    Style_t DS = {
        .background_color = 0x00000000,
        .border_color = 0x00000000,
        .hover_color = 0x008080FF,
        .padding = 5,
        .border_width = 2,
    };
    Style_t SC = {
        .background_color = 0x006030FF,
        .border_color = 0xFFFFFFFF,
        .hover_color = 0x008080FF,
        .padding = 4,
        .border_width = 0,
    };
    Style_t HUD = {
        .background_color = 0x00000000,
        .border_color = 0x000000FF,
        .border_width = 0,
    };

    // Create a container for our drive buttons
    drive_container = new DrivesOSD_t(&ui_ctx, DC);
    drive_container->set_position(600, 140);
    drive_container->size(400, 575);
    drive_container->set_button_style(DS);
    drive_container->set_click_handler([this](StorageButton *button, const SDL_Event&) {
        const storage_key_t key = button->get_key();
        void (*click_fn)(void*) = (button->get_drive_type() == DRIVE_TYPE_PRODOS_BLOCK)
            ? bazfast_button_click
            : diskii_button_click;
        click_fn(new diskii_callback_data_t{this, key});
    });

    std::vector<drive_spec_t> drive_specs;
    for (const auto& drive : computer->mounts->get_all_drives()) {
        drive_specs.push_back({drive.key, drive.drive_type, drive.status});
    }
    drive_container->rebuild(drive_specs);
    containers.push_back(drive_container);

    /*
     instead of creating whole new buttons, we insert the same buttons into this container.
     this needs to be dynamic, based on which slot is active at any given time.
     Create HUD drive container for first DiskII slot found 
    */
    hud_drive_container = new DrivesHUD_t(&ui_ctx, HUD, computer->mounts);
    hud_drive_container->size(420, 120);
    ncontainers.push_back(hud_drive_container);

    slot_container = new SlotsPanel_t(&ui_ctx, SC, slot_manager_name_resolver(slot_manager));
    slot_container->set_position(30, 140);
    slot_container->size(320, 304);
    slot_container->layout();
    containers.push_back(slot_container);

    Style_t CB = config_selector_button_style();

    mon_color_con = new Container_t(&ui_ctx, SC);
    mon_color_con->set_position(30, 550);
    mon_color_con->size(320, 65);
    containers.push_back(mon_color_con);
    populate_display_selector(mon_color_con, &ui_ctx, CB);
    for (size_t i = 0; i < mon_color_con->count(); i++) {
        Tile_t *tile = mon_color_con->get_tile(i);
        tile->on_click([tile](const SDL_Event&) -> bool {
            getMenuInterface()->setMonitor(static_cast<int>(tile->value()));
            return true;
        });
    }

    speed_con = new Container_t(&ui_ctx, SC);
    speed_con->set_position(30, 475);
    speed_con->size(320, 65);
    containers.push_back(speed_con);
    SelectButton_t *speed_btns[5] = {};
    populate_speed_selector(speed_con, &ui_ctx, CB, speed_btns);
    speed_btn_10 = speed_btns[0];
    speed_btn_28 = speed_btns[1];
    speed_btn_71 = speed_btns[2];
    speed_btn_14 = speed_btns[3];
    speed_btn_8 = speed_btns[4];
    speed_btn_10->on_click([this](const SDL_Event&) -> bool {
        this->clock->set_clock_mode(CLOCK_1_024MHZ);
        return true;
    });
    speed_btn_28->on_click([this](const SDL_Event&) -> bool {
        this->clock->set_clock_mode(CLOCK_2_8MHZ);
        return true;
    });
    speed_btn_71->on_click([this](const SDL_Event&) -> bool {
        this->clock->set_clock_mode(CLOCK_7_159MHZ);
        return true;
    });
    speed_btn_14->on_click([this](const SDL_Event&) -> bool {
        this->clock->set_clock_mode(CLOCK_14_3MHZ);
        return true;
    });
    speed_btn_8->on_click([this](const SDL_Event&) -> bool {
        this->clock->set_clock_mode(CLOCK_FREE_RUN);
        return true;
    });

    // Create text buttons for the disk save dialog
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;

    close_btn = new Button_t(&ui_ctx, "<", TextButtonCfg);
    close_btn->on_click([this](const SDL_Event& event) -> bool {
        close_panel();
        return true;
    });
    close_btn->size(36, 36);
    close_btn->set_position(window_w-100, 49);

    open_btn = new FadeButton_t(&ui_ctx, ">", TextButtonCfg);
    open_btn->on_click([this](const SDL_Event& event) -> bool {
        open_panel();
        return true;
    });
    open_btn->size(36, 36);
    open_btn->set_position(0, 50);
    open_btn->set_fade_frames(512, 4); // hold for one second, then fade out over next second. (roughly)

    Style_t SB;
    SB.background_color = 0x00000000;
    SB.border_width = 0;
    SB.border_color = 0x000000FF;
    SB.padding = 0;

    hover_controls_con = new HoverControls_t(&ui_ctx, SB, clock);
    ncontainers.push_back(hover_controls_con);

    system_config = computer->get_system();
    system_badge = new SystemBadge_t(&ui_ctx, computer->platform->image_id, SB,
        system_config->name ? system_config->name : "",
        system_config->description ? system_config->description : "");
    system_badge->set_position(30, 65);

    status_message = new StatusMessage_t(&ui_ctx);
    ncontainers.push_back(status_message);

    computer->sys_event->registerHandler(SDL_EVENT_DROP_BEGIN, [this](const SDL_Event &event) {
        SDL_RaiseWindow(window);
        slideStatusBeforeDrop = currentSlideStatus;
        if (currentSlideStatus == SLIDE_OUT) {
            open_panel();
        }
        return true;
    });
    computer->sys_event->registerHandler(SDL_EVENT_DROP_FILE, [this,computer](const SDL_Event &event) {
        printf("SDL_EVENT_DROP_FILE: %s\n", event.drop.data);
        if (event.drop.data) {
            const ConfigFileKind kind = detect_config_file_kind(event.drop.data);
            if (kind == ConfigFileKind::Gs2 || kind == ConfigFileKind::Settings) {
                computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Quit emulation first"));
                return true;
            }
        }
#if 0
        // Identify the media type to help control what buttons we allow to highlight.
        // TODO: there's little point in doing this here, the intention was to filter based on drive type but SDL can't do that yet.
        media_descriptor *media = new media_descriptor();
        media->filename = event.drop.data;
        if (identify_media(*media) != 0) { // if this is unrecognized media, don't allow it to be dropped.
            delete media;
            computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Unrecognized media type"));
            return false;
        }
        delete media;
#endif
        // after that, find button that was under the mouse. Scan Drive Container for button that is highlighted.
        for (int i = 0; i < drive_container->count(); i++) {
            Tile_t *tile = drive_container->get_tile(i);
            if (tile && tile->is_mouse_hovering()) {
                StorageButton *button = dynamic_cast<StorageButton *>(tile);
                storage_key_t key = button->get_key();
                disk_mount_t dm;
                dm.filename = strndup(event.drop.data, 1024);
                dm.slot = key.slot;
                dm.drive = key.drive;   
                bool result = computer->mounts->mount_media(dm);
                if (!result) {
                    computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Failed to mount media"));
                } else {
                    // this isn't displaying because the message was rendered into the local stack frame, duh.
                    static char msg[160];
                    snprintf(msg, sizeof(msg), "Mounted media %s", event.drop.data);
                    computer->event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, msg));            
                }
            }
        }
        // Raise our window to the top. After a DD I would expect to be able to just go back to doing stuff in the app.
        SDL_RaiseWindow(window);
#if defined(__EMSCRIPTEN__)
        // On the web, DROP_FILE arrives asynchronously *after* DROP_COMPLETE,
        // so we do the deferred panel close here (the COMPLETE handler is a
        // no-op on web). slideStatusBeforeDrop holds the pre-drag state.
        if (web_drag_active) {
            if (slideStatusBeforeDrop == SLIDE_OUT) {
                close_panel();
            }
            web_drag_active = false;
        }
#endif
        return true;
    });

    computer->sys_event->registerHandler(SDL_EVENT_DROP_COMPLETE, [this](const SDL_Event &event) {
#if defined(__EMSCRIPTEN__)
        // The synchronous DROP_COMPLETE fires before the async DROP_FILE on the
        // web; closing the panel now would collapse the hover target before the
        // file lands. The DROP_FILE handler performs the close instead.
        return true;
#else
        if (slideStatusBeforeDrop == SLIDE_OUT) {
            close_panel();
        }
        return true;
#endif
    });
}

OSD::~OSD() {
    SDL_DestroyTexture(cpTexture);
    delete text_render;
    delete title_trender;
    delete system_badge;
    for (Container_t* container : containers) {
        delete container;
    }
    for (Container_t* container : ncontainers) {
        delete container;
    }
    //delete diskii_save_con;
    delete close_btn;
    delete open_btn;
}

void OSD::update() {

    if (!mstack.stack.empty()) {
        mstack.stack.top()->update();
        if (mstack.stack.top()->is_completed()) {
            //delete modal_stack.stack.top();
            mstack.stack.pop();
        }
    }

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
    }

    // update disk status - iterate over all drives based on what's in slots
    for (int i = 0; i < drive_container->count(); i++) {
        Tile_t *tile = drive_container->get_tile(i);
        if (tile) {
            StorageButton *button = dynamic_cast<StorageButton *>(tile);
            storage_key_t key = button->get_key();
            drive_status_t ds = computer->mounts->media_status(key);
            button->set_disk_status(ds);
        }
    }

    hud_drive_container->set_visible((currentSlideStatus == SLIDE_OUT) ? true : false);

    speed_con->selected_value(getMenuInterface()->getCurrentSpeed());
    //hov_speed->set_assetID(speed_asset.at(getMenuInterface()->getCurrentSpeed()));
    //hov_speed_con->selected_value(getMenuInterface()->getCurrentSpeed());
    mon_color_con->selected_value(getMenuInterface()->getCurrentMonitor());

    // it can be visible but transparent at same time. visible means it can be hit-tested and displayed.
/*     if (is_mouse_captured()) {
        hover_controls_con->set_visible(false);
    } else {
        hover_controls_con->set_visible(true);
    }
 */
    for (Container_t* container : ncontainers) {
        container->update();
    }

    computer->video_system->osd_control_panel_open = requires_host_cursor();
}

void OSD::set_heads_up_message(const std::string &text, int count) {
    status_message->trigger(text);
}

/** Draw the control panel (if visible) */
void OSD::render() {
    int window_width, window_height;
    SDL_GetWindowSize(window, &window_width, &window_height);
    
    // save the current rendering scale
    float ox,oy;
    //SDL_GetRenderScale(renderer, &ox, &oy);

    // assume everything in this routine is drawn at 1:1 scale.
    //SDL_SetRenderScale(renderer, 1.0,1.0); // TODO: calculate these based on window size

    /** if current Status is out, don't draw. If status is in transition or IN, draw. */
    if (currentSlideStatus == SLIDE_IN || (slideStatus && (currentSlideStatus != slideStatus))) {

        /* ----- */
        /* Redraw the whole control panel from bottom up, because the modal could have been anywhere! */
        SDL_SetRenderTarget(renderer, cpTexture);
        
        //ui_ctx.color(0x000000FF);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // make the background opaque and black.
        ui_ctx.fill_rect({0, 0, (float)window_w, (float)window_h}, 0x00000000);
 
        // Draw CP background with some opacity
        SDL_FRect rect = {0, 50, (float)(window_w-100), (float)(window_h-100)};
        //ui_ctx.fill_rect(rect, 0xFFFFFFE0);
        ui_ctx.fill_rect(rect, computer->platform->case_color & 0xFFFFFF00 | 0xE0);
      
        ui_ctx.fill_rect({0, 130, (float)(window_w-100), 3}, 0x000000FF);

        /* ----- */

        SDL_FRect cpTargetRect = {
            (float)slidePosition,
            (float)0, // no vertical offset
            (float)window_w+slidePosition, 
            (float)window_h
        };

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        system_badge->render();

        close_btn->render();

        // re-lay this out since the hud might have changed the buttons.
        drive_container->layout();

        // Render the container and its buttons into the cpTexture
        SDL_SetRenderTarget(renderer, cpTexture);
        for (Container_t* container : containers) {
            container->render();
        }

        if (!mstack.stack.empty()) {
            mstack.stack.top()->render();
        }
/*         if (activeModal) {
            activeModal->render();
        } */

        SDL_SetRenderTarget(renderer, nullptr);

        // now render the cpTexture into window
        SDL_RenderTexture(renderer, cpTexture, NULL, &cpTargetRect);
    }

    /* if (activeModal) {
        activeModal->render();
    } */
    if (!mstack.stack.empty()) {
        mstack.stack.top()->render();
    }

    // for each item in ncontainers, call render()
    // As I move functions out of OSD into their own classes, I need to add them to ncontainers
    // so they can be rendered.
    for (Container_t* container : ncontainers) {
        container->render();
    }

    if (currentSlideStatus == SLIDE_OUT) {

        open_btn->render(); // this now takes care of its own fade-out.

        //hover_controls_con->render();

        // display the MHz at the bottom of the screen.
        {
            char hud_str[150];
            snprintf(hud_str, sizeof(hud_str), "MHz: %8.4f / FPS %8.4f / Idle: %5.1f%%", computer->e_mhz, computer->fps, computer->get_idle_percent());
            SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
            SDL_RenderDebugText(renderer, 20, window_height - 30, hud_str);

            display_state_t *ds = (display_state_t *)computer->cached_display_state;
            //if (clock->get_video_scanner()) {
                //VideoScannerII *vs = clock->get_video_scanner();
                VideoScannerII *vs = ds->video_scanner;
                snprintf(hud_str, sizeof(hud_str), "H: %3d V: %3d c: %6d (SB: %d)", 
                    vs->get_hcount(), 
                    vs->get_vcount(), 
                    vs->get_scan_cycle(),
                    ds->video_scanner->get_frame_scan()->get_count()
                    /* vs->get_frame_scan()->get_count() */);
                SDL_RenderDebugText(renderer, 20, window_height - 50, hud_str);
            //}            
        }
    }
    // Draw the platform menu overlay (Linux: ☰ hamburger button) at 1:1 scale
    renderMenuOverlay(renderer, window_width, window_height);

    // Restore scale
    //SDL_SetRenderScale(renderer, ox,oy);
    // set draw color to black - why?
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
}

bool OSD::is_mouse_captured() {
    return SDL_GetWindowRelativeMouseMode(window);
}

bool OSD::event(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT) {
        mstack.stack.push(new QuitModal_t(&ui_ctx, "Are you sure you want to quit?", ModalStyle, event_queue, computer->mounts, mstack));
        computer->video_system->osd_control_panel_open = true;
        return true;
    }

    //if mouse is captured we ignore events here.
    if (is_mouse_captured()) {
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            return(false);
        }
        hover_controls_con->reset();
    }

    bool active = (currentSlideStatus == SLIDE_IN);

    // Modal intercepts all mouse events regardless of whether the panel is open.
    // The modal renders directly to the window (not just inside the panel), so
    // its event handling must also work when the panel is closed.
    if (!mstack.stack.empty()) {
        mstack.stack.top()->handle_mouse_event(event);
        /* if (modal_stack.stack.top()->is_completed()) {
            delete modal_stack.stack.top();
            modal_stack.stack.pop();
        } */
        return true;
    }
    /* if (activeModal) {
        activeModal->handle_mouse_event(event);
        if (activeModal->is_completed()) {
            delete activeModal;
            activeModal = nullptr;
        }
        return true;
    } */

    if (active) {
        close_btn->handle_mouse_event(event);
        // Let containers have a stab at the event
        for (Container_t* container : containers) {
            if (container->handle_mouse_event(event)) break;
        }
    } else {
        if (open_btn->handle_mouse_event(event)) {
            return(true);
        }
        
        /* if (! is_mouse_captured()) {
            if (hover_controls_con->handle_mouse_event(event)) return(true); // only handles if still visible.
        } */
        //hov_speed_con->handle_mouse_event(event);
        for (Container_t* container : ncontainers) {
            if (container->handle_mouse_event(event) && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                return(true);
            }
        }
    }

    switch (event.type)
    {
        case SDL_EVENT_KEY_DOWN:
            if (event.key.key == SDLK_F4) {
                return(true);
            }
            break;
        case SDL_EVENT_KEY_UP:
            //printf("osd key down: %d %d %d\n", event.key.key, slideStatus, currentSlideStatus);
            if (event.key.key == SDLK_F4) {
                //SDL_SetWindowRelativeMouseMode(window, false);
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
        case SDL_EVENT_DROP_POSITION: {
#if defined(__EMSCRIPTEN__)
                // The web backend never sends DROP_BEGIN, so synthesize it on
                // the first position event of a drag: snapshot the panel state
                // and slide it out so a drive button can be hovered.
                if (!web_drag_active) {
                    web_drag_active = true;
                    SDL_RaiseWindow(window);
                    slideStatusBeforeDrop = currentSlideStatus;
                    if (currentSlideStatus == SLIDE_OUT) {
                        open_panel();
                    }
                }
#endif
                // the specific buttons that can drag/drop need to handle this event.
                for (Container_t* container : containers) {
                    container->handle_mouse_event(event);
                }
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
    computer->video_system->osd_control_panel_open = true;
    computer->video_system->push_mouse_capture(false);
}

void OSD::close_panel() {
    slideStatus = SLIDE_OUT;   
    slidePosition = 0;
    slidePositionDelta = slidePositionDeltaMin;
    computer->video_system->pop_mouse_capture();
}

// TODO: refactor these to push/pop from the modal stack.
void OSD::show_diskii_modal(storage_key_t key, uint64_t data) {
    ModalContainer_t *diskii_save_con = new DirtyDiskSave_t(&ui_ctx, nullptr, ModalStyle, key, computer->mounts, mstack);
    mstack.stack.push(diskii_save_con);
    computer->video_system->osd_control_panel_open = true;
    diskii_save_con->set_key(key);
    diskii_save_con->set_data(data);
}

bool OSD::check_for_dirty_disks() {
//                 if (!osd->check_for_dirty_disks()) {
    const std::vector<drive_info_t>& drives = computer->mounts->get_all_drives();

    for (const drive_info_t& drive : drives) {
        if (drive.status.is_modified) {
            show_diskii_modal(drive.key, 0);
            return true; // only show one at a time.
        }
    }
    return false;
}
