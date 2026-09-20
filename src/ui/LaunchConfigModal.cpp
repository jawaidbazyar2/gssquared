#include "LaunchConfigModal.hpp"
#include <SDL3/SDL.h>
#include "util/mount.hpp"

LaunchConfigModal_t::LaunchConfigModal_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style,
    Mounts *mounts, modal_stack &stack, std::function<void()> on_launch) :
        ModalContainer_t(ctx, msg_text, initial_style, stack), mounts(mounts),
        on_launch(std::move(on_launch)) {

    if (msg_text == nullptr) this->msg_text = std::string("Launch this config? Current machine will stop.");

    int window_w, window_h;
    SDL_GetWindowSize(ctx->window, &window_w, &window_h);
    set_position((window_w - 500) / 2, (window_h - 200) / 2);
    size(500, 200);

    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;

    launch_btn = new Button_t(ctx, "Launch", TextButtonCfg);
    cancel_btn = new Button_t(ctx, "Cancel", TextButtonCfg);

    launch_btn->size(100, 30);
    cancel_btn->size(100, 30);

    launch_btn->on_click([this](const SDL_Event& event) -> bool {
        state = COMPLETED;
        check_for_dirty_disks();
        return true;
    });

    cancel_btn->on_click([this](const SDL_Event& event) -> bool {
        state = CANCELED;
        completed = true;
        return true;
    });
    state = WAIT_FOR_YES_NO;

    add(launch_btn);
    add(cancel_btn);
    layout();
}

bool LaunchConfigModal_t::check_for_dirty_disks() {

    const std::vector<drive_info_t>& drives = mounts->get_all_drives();

    for (const drive_info_t& drive : drives) {
        if (drive.status.is_modified) {
            save_modal = new DirtyDiskSave_t(ctx, nullptr, style, drive.key, mounts, stack);
            stack.stack.push(save_modal);
            state = WAIT_FOR_SAVE;
            save_modal->set_key(drive.key);
            save_modal->set_data(0);
            return true;
        }
    }
    return false;
}

void LaunchConfigModal_t::update() {
    if (state == WAIT_FOR_YES_NO) {
    } else if (state == COMPLETED) {
        if (on_launch) {
            on_launch();
        }
        completed = true;
        return;
    } else if (state == WAIT_FOR_SAVE) {
        if (save_modal->is_completed()) {
            if (save_modal->is_canceled()) {
                state = CANCELED;
                delete save_modal;
                save_modal = nullptr;
                return;
            }

            delete save_modal;
            save_modal = nullptr;
            state = COMPLETED;
            check_for_dirty_disks();
        }
    } else if (state == CANCELED) {
        completed = true;
        return;
    }
}
