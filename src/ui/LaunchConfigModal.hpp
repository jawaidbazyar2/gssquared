#pragma once

#include <functional>
#include "ModalContainer.hpp"
#include "UIContext.hpp"
#include "Button.hpp"
#include "DirtyDiskSave.hpp"

class LaunchConfigModal_t : public ModalContainer_t {
    enum LaunchConfigModalState_t {
        WAIT_FOR_YES_NO,
        NORMAL,
        WAIT_FOR_SAVE,
        COMPLETED,
        CANCELED,
    };

protected:
    Button_t *launch_btn = nullptr;
    Button_t *cancel_btn = nullptr;
    DirtyDiskSave_t *save_modal = nullptr;
    Mounts *mounts = nullptr;
    LaunchConfigModalState_t state = NORMAL;
    std::function<void()> on_launch;

public:
    LaunchConfigModal_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style,
        Mounts *mounts, modal_stack &stack, std::function<void()> on_launch);
    virtual void update() override;
    bool check_for_dirty_disks();
};
