#pragma once

#include "ModalContainer.hpp"
#include "UIContext.hpp"
#include "Button.hpp"
#include "util/EventQueue.hpp"
#include "DirtyDiskSave.hpp"

class QuitModal_t : public ModalContainer_t {
    enum QuitModalState_t {
        WAIT_FOR_YES_NO,
        NORMAL,
        WAIT_FOR_SAVE,
        COMPLETED,
        CANCELED,
    };

protected:
    Button_t *yes_btn = nullptr;
    Button_t *no_btn = nullptr;
    EventQueue *event_queue = nullptr;
    DirtyDiskSave_t *save_modal = nullptr;
    Mounts *mounts = nullptr;
    QuitModalState_t state = NORMAL;

public:
    QuitModal_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, 
        EventQueue *event_queue, Mounts *mounts, modal_stack &stack);
    virtual void update() override;
    bool check_for_dirty_disks();
};