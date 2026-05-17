#include "DirtyDiskSave.hpp"
#include "util/mount.hpp"

DirtyDiskSave_t::DirtyDiskSave_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, 
    storage_key_t key, Mounts *mounts, modal_stack &stack) : 
        ModalContainer_t(ctx, msg_text, initial_style, stack), key(key), mounts(mounts) {

    // get window size
    int window_w, window_h;
    SDL_GetWindowSize(ctx->window, &window_w, &window_h);
    set_position((window_w - 500) / 2, (window_h - 200) / 2);
    size(500, 200);

    if (msg_text == nullptr) this->msg_text = std::string("Disk Data has been modified. Save?");

    // get the slot/drive from the key, and get filename from mounts, to display under msg_text below.
    char slot_drive[32];
    snprintf(slot_drive, sizeof(slot_drive), "%d/%d", key.slot, key.drive+1);

    filename = std::string(slot_drive) + " " + mounts->media_status(key).filename;
    if (filename.empty()) filename = "(empty)";
    
    // Create text buttons for the disk save dialog
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;
    
    save_btn = new Button_t(ctx, "Save", TextButtonCfg);
    //save_as_btn = new Button_t(ctx, "Save As", TextButtonCfg);
    discard_btn = new Button_t(ctx, "Discard", TextButtonCfg);
    cancel_btn = new Button_t(ctx, "Cancel", TextButtonCfg);
    
    save_btn->size(100, 30);
    //save_as_btn->size(100, 30);
    discard_btn->size(100, 30);
    cancel_btn->size(100, 30);

    /*
    these just do:
        osd->event_queue->addEvent(new Event(EVENT_MODAL_CLICK, container->get_key().key, d->key.key));

        instead of EVENT_MODAL_CLICK (which modal??)
        generate more specific events:
            writeback
        or just do the things here? is there any reason to do it in frame_appevent?

    */
    save_btn->on_click([this](const SDL_Event& event) -> bool {
        this->mounts->unmount_media(this->key, SAVE_AND_UNMOUNT);
        completed = true;
        return true;
    });
    discard_btn->on_click([this](const SDL_Event& event) -> bool {
        this->mounts->unmount_media(this->key, DISCARD);
        completed = true;
        return true;
    });
    cancel_btn->on_click([this](const SDL_Event& event) -> bool {
        // do something to pop us off the modal stack.
        canceled = true;
        completed = true;
        return true;
    });
    add(save_btn);

    add(discard_btn);
    add(cancel_btn);
    layout();
}

void DirtyDiskSave_t::render() {
    ModalContainer_t::render();
    ctx->text_render->render(filename, tp.x + 250, tp.y + 65, TEXT_ALIGN_CENTER);
}
