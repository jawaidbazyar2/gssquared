#include "QuitModal.hpp"
#include "util/mount.hpp"
#include "util/Event.hpp"

QuitModal_t::QuitModal_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, 
    EventQueue *event_queue, Mounts *mounts, modal_stack &stack) : 
        ModalContainer_t(ctx, msg_text, initial_style, stack), event_queue(event_queue), mounts(mounts) {

    if (msg_text == nullptr) this->msg_text = std::string("Are you sure you want to quit?");

    // get window size
    int window_w, window_h;
    SDL_GetWindowSize(ctx->window, &window_w, &window_h);
    set_position((window_w - 500) / 2, (window_h - 200) / 2);
    size(500, 200);

    // Create text buttons for the disk save dialog
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;
    
    yes_btn = new Button_t(ctx, "Yes", TextButtonCfg);
    no_btn = new Button_t(ctx, "No", TextButtonCfg);
    
    yes_btn->size(100, 30);
    no_btn->size(100, 30);

    yes_btn->on_click([this](const SDL_Event& event) -> bool {
        state = COMPLETED;
        check_for_dirty_disks();
        return true;
    });

    no_btn->on_click([this](const SDL_Event& event) -> bool {
        state = CANCELED;
        completed = true;
        return true;
    });
    state = WAIT_FOR_YES_NO;

    add(yes_btn);
    add(no_btn);
    layout();
}

bool QuitModal_t::check_for_dirty_disks() {

    const std::vector<drive_info_t>& drives = mounts->get_all_drives();

    for (const drive_info_t& drive : drives) {
        if (drive.status.is_modified) {
            //show_diskii_modal(drive.key, 0);
            save_modal = new DirtyDiskSave_t(ctx, nullptr, style, drive.key, mounts, stack);
            stack.stack.push(save_modal);
            state = WAIT_FOR_SAVE;
            save_modal->set_key(drive.key);
            save_modal->set_data(0);
            return true; // only show one at a time.
        }
    }
    return false;
}

void QuitModal_t::update() {
    if (state == WAIT_FOR_YES_NO) { // do nothing
    } else if (state == COMPLETED) {
        event_queue->addEvent(new Event(EVENT_QUIT, 0u, (uint64_t)0));
        completed = true;
        return;
    } else if (state == WAIT_FOR_SAVE) {
        if (save_modal->is_completed()) {
            // look inside save modal to see if it was canceled, if so set our state to CANCELED
            if (save_modal->is_canceled()) {
                state = CANCELED;
                delete save_modal;
                save_modal = nullptr;
                return;
            }
            
            delete save_modal;
            save_modal = nullptr;
            state = COMPLETED; // might get overridden by check_for_dirty_disks()
            check_for_dirty_disks();
        }       
    } else if (state == CANCELED) {
        completed = true;
        return;
    }
}