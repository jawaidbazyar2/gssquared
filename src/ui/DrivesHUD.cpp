#include "Container.hpp"
#include "DrivesHUD.hpp"
#include "StorageButton.hpp"
#include "StorageButtonFactory.hpp"
#include "util/mount.hpp"

DrivesHUD_t::DrivesHUD_t(UIContext *ctx, const Style_t& style, Mounts *mounts) : Container_t(ctx, style), mounts(mounts) {
    // empty
     // New Mounts-based button creation
    // Get all registered drives from the Mounts system
    const std::vector<drive_info_t>& drives = mounts->get_all_drives();

    // Create buttons for each registered drive
    for (const auto& drive : drives) {
        StorageButton *button = create_storage_button(ctx, drive.drive_type, style);
        button->set_drive_type(drive.drive_type);
        button->set_key(drive.key);
        buttons.push_back(button);
    }
}

void DrivesHUD_t::render() {
    if (!visible) return;

    if (tiles.size() > 0) {

        // display running disk drives at the bottom of the screen.
        SDL_SetRenderDrawBlendMode(ctx->renderer, SDL_BLENDMODE_BLEND);

        Container_t::render();
    }
}

void DrivesHUD_t::update() {
    if (!visible) return;

    int window_width, window_height;
    SDL_GetWindowSize(ctx->window, &window_width, &window_height);

    drive_type_t drive_type_active = DRIVE_TYPE_DISKII;

    // update disk status - iterate over all drives based on what's in slots
    uint16_t key_slot_match = 0;

    // two pass. First, update all buttons and calculate key mask. (the lit drive could have been the
    // 2nd in a pair, hence 2-pass.)
    drive_status_t ds;
    for (StorageButton *button : buttons) {
        storage_key_t key = button->get_key();
        ds = mounts->media_status(key);
        button->set_disk_status(ds);
        if (ds.motor_on) {
            key_slot_match = key.slot;
            drive_type_active = button->get_drive_type();
        }
    }

    float max_height = 0;
    int drives_active = 0;

    // update the HUD container.
    remove_all_tiles(); // always clear.. 
    if (key_slot_match) {
        // second pass, update the hud container with items matching the key mask.
        // and set their hover status to false.
        for (StorageButton *button : buttons) {
            storage_key_t key = button->get_key();
            ds = button->get_disk_status();
            if (key.slot == key_slot_match) {
                // if the device is a HD20SC, and it's not the active one, don't display it.
                if (button->get_drive_type() == DRIVE_TYPE_PRODOS_BLOCK && !button->get_disk_status().motor_on) {
                    continue;
                }
                add(button);
                button->set_active(false);
                float tile_width, tile_height;
                button->get_tile_size(&tile_width, &tile_height);
                max_height = std::max(max_height, tile_height);
                drives_active++;
            }
        }
    }
    if (drive_type_active == DRIVE_TYPE_PRODOS_BLOCK) {
        drives_x = ((float)window_width - 210*drives_active) / 2;
    } else {
        drives_x = ((float)window_width - 420) / 2;
    }
    
    drives_y = window_height - max_height - 15; // adjust for height of the tiles, align to bottom
    set_position(drives_x, drives_y );
    calc_content_position();
    layout();
}
