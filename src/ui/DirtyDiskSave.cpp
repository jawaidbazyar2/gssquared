#include "DirtyDiskSave.hpp"
#include "util/mount.hpp"

DirtyDiskSave_t::DirtyDiskSave_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, storage_key_t key,Mounts *mounts) : 
        ModalContainer_t(ctx, msg_text, initial_style), key(key), mounts(mounts) {

    set_position(300, 200);
    size(500, 200);


    // Create text buttons for the disk save dialog
    Style_t TextButtonCfg;
    TextButtonCfg.background_color = 0xE0E0FFFF;
    TextButtonCfg.text_color = 0x000000FF;
    TextButtonCfg.border_width = 1;
    TextButtonCfg.border_color = 0x000000FF;
    TextButtonCfg.padding = 2;
    
    save_btn = new Button_t(ctx, "Save", TextButtonCfg);
    save_as_btn = new Button_t(ctx, "Save As", TextButtonCfg);
    discard_btn = new Button_t(ctx, "Discard", TextButtonCfg);
    cancel_btn = new Button_t(ctx, "Cancel", TextButtonCfg);
    
    save_btn->size(100, 30);
    save_as_btn->size(100, 30);
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
        completed = true;
        return true;
    });
    add(save_btn);

    add(discard_btn);
    add(cancel_btn);
    layout();
}

DirtyDiskSave_t::~DirtyDiskSave_t() {
    delete save_btn;
    delete save_as_btn;
    delete discard_btn;
    delete cancel_btn;
}