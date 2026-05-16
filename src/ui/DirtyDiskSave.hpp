#pragma once

#include "ModalContainer.hpp"
#include "UIContext.hpp"
#include "Button.hpp"
#include "util/mount.hpp"

class DirtyDiskSave_t : public ModalContainer_t {
protected:
    Button_t *save_btn = nullptr;
    Button_t *save_as_btn = nullptr;
    Button_t *discard_btn = nullptr;
    Button_t *cancel_btn = nullptr;
    storage_key_t key;
    Mounts *mounts;
public:
    DirtyDiskSave_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, storage_key_t key, Mounts *mounts);
    ~DirtyDiskSave_t();
};