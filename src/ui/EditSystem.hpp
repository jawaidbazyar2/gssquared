/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   System config editor (SelectSystem sibling). Edits a ConfigDraft without
 *   powering on the emulator.
 */

#pragma once

#include <SDL3/SDL.h>
#include <string>

#include "AssetAtlas.hpp"
#include "Container.hpp"
#include "DrivesOSD.hpp"
#include "ModalContainer.hpp"
#include "SlotsPanel.hpp"
#include "SystemBadge.hpp"
#include "TextInput.hpp"
#include "UIContext.hpp"
#include "util/ConfigDraft.hpp"
#include "util/TextRenderer.hpp"
#include "videosystem.hpp"

#define EDIT_PENDING 0
#define EDIT_CANCEL  1
#define EDIT_SAVED   2
#define EDIT_QUIT    3

class EditSystem {
protected:
    video_system_t *vs = nullptr;
    AssetAtlas_t *aa = nullptr;
    UIContext ui_ctx{};
    TextRenderer *text_renderer = nullptr;
    TextRenderer *title_renderer = nullptr;

    ConfigDraft draft;
    int result = EDIT_PENDING;
    bool updated = true;
    int design_width = 1288;
    int design_height = 928;
    /** Offset applied to all content except the window title (centers the content block). */
    float layout_dx = 0.0f;
    float layout_dy = 0.0f;
    /** Extra Y offset for content below the badge / name fields. */
    float body_dy = 20.0f;

    SystemBadge_t *badge = nullptr;
    TextInput_t *name_input = nullptr;
    TextInput_t *desc_input = nullptr;
    SlotsPanel_t *slots_panel = nullptr;
    DrivesOSD_t *drives_panel = nullptr;
    Container_t *speed_con = nullptr;
    Container_t *display_con = nullptr;
    Container_t *platform_con = nullptr;
    Container_t *action_con = nullptr;
    Button_t *save_btn = nullptr;
    Button_t *cancel_btn = nullptr;

    modal_stack mstack;
    int picking_slot = -1;
    Container_t *card_picker = nullptr;

    std::string status_text;
    storage_key_t pending_mount_key{};

    void refresh_badge();
    void commit_text_fields();
    /** Route mouse/key events to name/desc fields; returns true if consumed. */
    bool handle_text_field_event(const SDL_Event& ev);
    void show_card_picker(int slot);
    void dismiss_card_picker();
    void open_premount_dialog(storage_key_t key);
    /** Open save dialog; on success sets EDIT_SAVED, on cancel stays in editor. */
    void begin_save();
    /** Write draft to path; returns false and sets status_text on failure. */
    bool write_draft_to_path(const std::string& path);
    std::string default_save_path() const;

public:
    void rebuild_ui_from_draft();

    EditSystem(video_system_t *vs, AssetAtlas_t *aa);
    ~EditSystem();

    void start_new(PlatformId_t platform_id = PLATFORM_APPLE_IIE_ENHANCED);
    void start_from_config(const SystemConfig& config);

    bool update();
    void render();
    bool event(const SDL_Event &event);

    int get_result() const { return result; }
    const ConfigDraft& get_draft() const { return draft; }
    ConfigDraft& get_draft() { return draft; }
};
