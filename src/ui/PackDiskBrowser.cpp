/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "PackDiskBrowser.hpp"

#include "Button.hpp"
#include "OSD.hpp"
#include "computer.hpp"
#include "util/Gs2Pack.hpp"
#include "util/mount.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::string mounted_on(Mounts *mounts, const std::string& image) {
    if (mounts == nullptr) {
        return {};
    }
    std::string where;
    for (const drive_info_t& drive : mounts->get_all_drives()) {
        if (!drive.status.is_mounted || drive.status.filename.empty()) {
            continue;
        }
        std::error_code ec;
        if (!std::filesystem::equivalent(drive.status.filename, image, ec)) {
            continue;
        }
        if (!where.empty()) {
            where += ", ";
        }
        where += "S" + std::to_string(drive.key.slot) + " D" + std::to_string(drive.key.drive + 1);
    }
    return where;
}

}  // namespace

PackDiskBrowser_t::PackDiskBrowser_t(UIContext *ctx, const Style_t& initial_style, modal_stack& stack,
                                     OSD *osd, storage_key_t key)
    : ModalContainer_t(ctx, "Disks in this pack", initial_style, stack) {
    this->key = key;

    int window_w = 800;
    int window_h = 600;
    if (ctx->window) {
        SDL_GetWindowSize(ctx->window, &window_w, &window_h);
    }

    gs2pack::Session *session = gs2pack::active();
    const std::vector<std::string> images = session ? gs2pack::list_disk_images(*session) : std::vector<std::string>{};
    const float row_h = 32.f;
    const float width = 520.f;
    const float height = 88.f + static_cast<float>(images.size()) * (row_h + 6.f) + 78.f;
    set_position((window_w - width) / 2.f, (window_h - height) / 2.f);
    size(width, height);

    Style_t button_style;
    button_style.background_color = 0xE0E0FFFF;
    button_style.text_color = 0x000000FF;
    button_style.border_width = 1;
    button_style.border_color = 0x000000FF;
    button_style.padding = 2;

    Mounts *mounts = (osd && osd->computer) ? osd->computer->mounts : nullptr;
    for (const std::string& image : images) {
        const std::string filename = std::filesystem::path(image).filename().string();
        const std::string where = mounted_on(mounts, image);
        const std::string label = where.empty() ? filename : filename + "  (" + where + ")";
        Button_t *button = new Button_t(ctx, label, button_style);
        button->size(width - 40.f, row_h);
        button->on_click([this, osd, image](const SDL_Event&) -> bool {
            gs2pack::Session *active = gs2pack::active();
            if (osd == nullptr || osd->computer == nullptr || osd->computer->mounts == nullptr || active == nullptr) {
                completed = true;
                return true;
            }
            disk_mount_t mount;
            mount.filename = image;
            mount.slot = this->key.slot;
            mount.drive = this->key.drive;
            if (!osd->computer->mounts->mount_media(mount)) {
                osd->set_heads_up_message("Failed to mount media", 512);
                completed = true;
                return true;
            }
            std::string error;
            if (!gs2pack::remember_mount(*active, this->key.slot, this->key.drive, image, error)) {
                std::cerr << "Failed to update pack config: " << error << "\n";
                osd->set_heads_up_message("Failed to update pack config", 180);
            }
            completed = true;
            return true;
        });
        add(button);
    }

    Button_t *local_btn = new Button_t(ctx, "Open from this computer...", button_style);
    local_btn->size(240.f, row_h);
    local_btn->on_click([this, osd](const SDL_Event&) -> bool {
        if (osd != nullptr) {
            osd->open_local_file_dialog(this->key);
        }
        completed = true;
        return true;
    });
    add(local_btn);

    Button_t *cancel_btn = new Button_t(ctx, "Cancel", button_style);
    cancel_btn->size(100.f, row_h);
    cancel_btn->on_click([this](const SDL_Event&) -> bool {
        canceled = true;
        completed = true;
        return true;
    });
    add(cancel_btn);

    layout();
}

void PackDiskBrowser_t::layout() {
    float y = tp.y + 52.f;
    const float left = tp.x + 20.f;
    const auto& children = get_tiles();
    for (size_t i = 0; i < children.size(); ++i) {
        Tile_t *tile = children[i];
        if (tile == nullptr || !tile->is_visible()) {
            continue;
        }
        tile->set_position(left, y);
        y += 38.f;
    }
}
