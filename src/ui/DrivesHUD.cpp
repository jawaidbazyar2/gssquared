#include "Container.hpp"
#include "DrivesHUD.hpp"
#include "DiskII_Button.hpp"
#include "AppleDisk_525_Button.hpp"
#include "Unidisk_Button.hpp"
#include "StorageButton.hpp"
#include "util/mount.hpp"

DrivesHUD_t::DrivesHUD_t(UIContext *ctx, const Style_t& style, Mounts *mounts) : Container_t(ctx, style), mounts(mounts) {
    // empty
     // New Mounts-based button creation
    // Get all registered drives from the Mounts system
    const std::vector<drive_info_t>& drives = mounts->get_all_drives();

    // Create buttons for each registered drive
    for (const auto& drive : drives) {
        uint8_t slot = drive.key.slot;
        uint8_t drive_num = drive.key.drive;
        StorageButton *button;

        // Create the appropriate button type based on drive_type
        if (drive.drive_type == DRIVE_TYPE_DISKII) {
            button = new DiskII_Button_t(ctx, 0, style);
        } else if (drive.drive_type == DRIVE_TYPE_APPLEDISK_525) {
            button = new AppleDisk_525_Button_t(ctx, 0, style);
        } else if (drive.drive_type == DRIVE_TYPE_PRODOS_BLOCK) {
            button = new Unidisk_Button_t(ctx, 0, style);
        }
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
    drives_x = ((float)window_width - 420) / 2;
    drives_y = window_height - 125;

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
        }
    }

    // update the HUD container.
    remove_all_tiles(); // always clear.. 
    if (key_slot_match) {
        // second pass, update the hud container with items matching the key mask.
        // and set their hover status to false.
        for (StorageButton *button : buttons) {
            storage_key_t key = button->get_key();
            ds = button->get_disk_status();
            if (key.slot == key_slot_match)
                add(button);
        }
    }
    set_position(drives_x, drives_y );
    calc_content_position();
    layout();
}