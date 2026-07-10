/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#include "EditSystem.hpp"

#include <cstdio>

#include "Button.hpp"
#include "ConfigSelectors.hpp"
#include "SelectButton.hpp"
#include "Style.hpp"
#include "TextInput.hpp"
#include "paths.hpp"
#include "platforms.hpp"
#include "util/SystemConfig.hpp"
#include "util/uuid.hpp"

#if defined(__EMSCRIPTEN__)
#include "platform-specific/emscripten/web_file_dialog.hpp"
#elif defined(__APPLE__)
#include "platform-specific/macos/gs2_save_dialog.hpp"
#endif

namespace {

Style_t panel_style() {
    return Style_t{
        .background_color = 0x006030FF,
        .border_color = 0xFFFFFFFF,
        .hover_color = 0x008080FF,
        .padding = 4,
        .border_width = 0,
    };
}

Style_t drive_style() {
    return Style_t{
        .background_color = 0x00000000,
        .border_color = 0x00000000,
        .hover_color = 0x008080FF,
        .padding = 5,
        .border_width = 2,
    };
}

Style_t text_btn_style() {
    return Style_t{
        .background_color = 0xE0E0FFFF,
        .border_color = 0x000000FF,
        .hover_color = 0x00C0C0FF,
        .padding = 4,
        .border_width = 1,
        .text_color = 0x000000FF,
    };
}

/** Card-picker choice buttons — saturated fill, white text, bright hover. */
Style_t card_picker_btn_style() {
    return Style_t{
        .background_color = 0x2E86ABFF, // ocean blue
        .border_color = 0xF0F7FAFF,
        .hover_color = 0xE36414FF,     // vivid orange
        .padding = 4,
        .border_width = 1,
        .text_color = 0xFFFFFFFF,
    };
}

/**
 * Platform SelectButtons: SelectButton_t overrides bg/text per state.
 * hover_color = steel blue (white text); unselected navy / selected amber
 * are set in SelectButton_t::calc_style.
 */
Style_t platform_btn_style() {
    return Style_t{
        .background_color = 0x1A1A2EFF,
        .border_color = 0xE8E8E8FF,
        .hover_color = 0x3D5A80FF,
        .padding = 4,
        .border_width = 1,
        .text_color = 0xFFFFFFFF,
    };
}

} // namespace

EditSystem::EditSystem(video_system_t *vs, AssetAtlas_t *aa)
    : vs(vs), aa(aa) {
    text_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 15.0f);
    title_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 24.0f);
    ui_ctx = {vs->renderer, vs->window, text_renderer, title_renderer, aa};

    design_width = vs->window_width > 0 ? vs->window_width : 1288;
    design_height = vs->window_height > 0 ? vs->window_height : 928;

    SDL_SetRenderLogicalPresentation(vs->renderer, design_width, design_height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // Content block in design coords before offset: x 30..900 (w=870). Center it;
    // shift everything except the title down 30px; panels below badge/fields +25 more.
    constexpr float kContentLeft = 30.0f;
    constexpr float kContentWidth = 870.0f;
    layout_dx = (design_width - kContentWidth) * 0.5f - kContentLeft;
    layout_dy = 30.0f;
    body_dy = layout_dy + 25.0f;

    Style_t SB{
        .background_color = 0x00000000,
        .border_color = 0x000000FF,
        .padding = 0,
        .border_width = 0,
    };
    badge = new SystemBadge_t(&ui_ctx, 0, SB, "", "");
    badge->set_draw_name(false);
    badge->set_draw_description(false);

    Style_t FieldStyle{
        .background_color = 0xFFFFFFF0,
        .border_color = 0x000000FF,
        .hover_color = 0xFFFFFFFF,
        .padding = 2,
        .border_width = 1,
        .text_color = 0x000000FF,
    };
    constexpr float kFieldH = 24.0f;
    constexpr float kFieldGap = 5.0f;
    constexpr float kNameY = 70.0f;
    const float desc_y = kNameY + kFieldH + kFieldGap;
    const float field_x = badge->get_text_x() + layout_dx;

    name_input = new TextInput_t(&ui_ctx, "", FieldStyle);
    name_input->set_text_renderer(text_renderer);
    name_input->set_max_length(64);
    name_input->size(520, kFieldH);
    name_input->set_position(field_x, kNameY + layout_dy);

    desc_input = new TextInput_t(&ui_ctx, "", FieldStyle);
    desc_input->set_text_renderer(text_renderer);
    desc_input->set_max_length(120);
    desc_input->size(520, kFieldH);
    desc_input->set_position(field_x, desc_y + layout_dy);

    // Center badge on the midline between the two fields (re-run in refresh_badge
    // after the platform image sets the badge height).
    const float fields_mid_y = kNameY + layout_dy + kFieldH + kFieldGap * 0.5f;
    float badge_w = 0, badge_h = 0;
    if (badge->get_badge()) {
        badge->get_badge()->get_tile_size(&badge_w, &badge_h);
    }
    badge->set_position(kContentLeft + layout_dx, fields_mid_y - badge_h * 0.5f);

    Style_t SC = panel_style();
    slots_panel = new SlotsPanel_t(&ui_ctx, SC, [this](int slot) {
        return draft.slot_device_name(slot);
    });
    slots_panel->set_position(30 + layout_dx, 140 + body_dy);
    slots_panel->size(320, 304);
    slots_panel->layout();

    for (int i = 0; i < NUM_SLOTS; i++) {
        SlotButton *btn = slots_panel->get_slot_button(i);
        if (!btn) continue;
        btn->on_click([this, i](const SDL_Event&) -> bool {
            show_card_picker(i);
            return true;
        });
    }

    Style_t DC{
        .background_color = 0x00000040,
        .border_color = 0xFFFFFF80,
        .hover_color = 0x008080FF,
        .padding = 4,
        .border_width = 2,
    };
    drives_panel = new DrivesOSD_t(&ui_ctx, DC);
    drives_panel->set_position(400 + layout_dx, 140 + body_dy);
    // Tall enough that Platform (below) can sit with its bottom on the Save/Cancel line.
    drives_panel->size(500, 450);
    drives_panel->set_button_style(drive_style());
    drives_panel->set_click_handler([this](StorageButton *button, const SDL_Event&) {
        storage_key_t key = button->get_key();
        if (button->get_disk_status().is_mounted) {
            draft.clear_mount(key.slot, key.drive);
            rebuild_ui_from_draft();
        } else {
            open_premount_dialog(key);
        }
    });

    Style_t CB = config_selector_button_style();
    speed_con = new Container_t(&ui_ctx, SC);
    speed_con->set_position(30 + layout_dx, 480 + body_dy);
    speed_con->size(320, 65);
    populate_speed_selector(speed_con, &ui_ctx, CB);
    for (size_t i = 0; i < speed_con->count(); i++) {
        Tile_t *tile = speed_con->get_tile(i);
        tile->on_click([this, tile](const SDL_Event&) -> bool {
            speed_con->selected_value(tile->value());
            updated = true;
            return true;
        });
    }

    display_con = new Container_t(&ui_ctx, SC);
    display_con->set_position(30 + layout_dx, 575 + body_dy);
    display_con->size(320, 65);
    populate_display_selector(display_con, &ui_ctx, CB);
    for (size_t i = 0; i < display_con->count(); i++) {
        Tile_t *tile = display_con->get_tile(i);
        tile->on_click([this, tile](const SDL_Event&) -> bool {
            display_con->selected_value(tile->value());
            updated = true;
            return true;
        });
    }

    platform_con = new Container_t(&ui_ctx, SC);
    // Bottom aligns with action_con (y=655, h=50 → 705) before body_dy.
    platform_con->set_position(400 + layout_dx, 625 + body_dy);
    platform_con->size(500, 80);
    for (int p = 0; p < PLATFORM_END; p++) {
        platform_info *plat = get_platform(p);
        if (!plat) continue;
        SelectButton_t *pb = new SelectButton_t(&ui_ctx, plat->name, platform_btn_style(), p);
        pb->size(150, 28);
        pb->on_click([this, p](const SDL_Event&) -> bool {
            draft.set_platform(static_cast<PlatformId_t>(p));
            rebuild_ui_from_draft();
            return true;
        });
        platform_con->add(pb);
    }
    platform_con->layout();

    action_con = new Container_t(&ui_ctx, SC);
    // Below Display (y=575, h=65 → ends ~640); leave a small gap.
    action_con->set_position(30 + layout_dx, 655 + body_dy);
    action_con->size(320, 50);
    save_btn = new Button_t(&ui_ctx, "Save", text_btn_style());
    save_btn->size(100, 36);
    save_btn->on_click([this](const SDL_Event&) -> bool {
        begin_save();
        return true;
    });
    cancel_btn = new Button_t(&ui_ctx, "Cancel", text_btn_style());
    cancel_btn->size(100, 36);
    cancel_btn->on_click([this](const SDL_Event&) -> bool {
        result = EDIT_CANCEL;
        updated = true;
        return true;
    });
    action_con->add(save_btn);
    action_con->add(cancel_btn);
    action_con->layout();

    start_new();
}

EditSystem::~EditSystem() {
    dismiss_card_picker();
    delete badge;
    delete name_input;
    delete desc_input;
    delete slots_panel;
    delete drives_panel;
    delete speed_con;
    delete display_con;
    delete platform_con;
    delete action_con;
    delete text_renderer;
    delete title_renderer;
}

void EditSystem::start_new(PlatformId_t platform_id) {
    result = EDIT_PENDING;
    draft.reset_for_platform(platform_id);
    rebuild_ui_from_draft();
}

void EditSystem::start_from_config(const SystemConfig& config) {
    result = EDIT_PENDING;
    draft.load_from(config);
    rebuild_ui_from_draft();
}

void EditSystem::refresh_badge() {
    platform_info *plat = get_platform(draft.config().platform_id);
    if (plat) {
        badge->set_image_id(plat->image_id);
    }
    badge->set_text(draft.name(), draft.description());

    // Keep badge vertically centered on the gap between name and description.
    if (name_input && desc_input && badge->get_badge()) {
        float name_x = 0, name_y = 0, name_w = 0, name_h = 0;
        float desc_x = 0, desc_y = 0, desc_w = 0, desc_h = 0;
        float badge_w = 0, badge_h = 0;
        name_input->get_tile_position(name_x, name_y);
        name_input->get_tile_size(&name_w, &name_h);
        desc_input->get_tile_position(desc_x, desc_y);
        desc_input->get_tile_size(&desc_w, &desc_h);
        badge->get_badge()->get_tile_size(&badge_w, &badge_h);
        const float mid_y = (name_y + name_h + desc_y) * 0.5f;
        badge->set_position(30 + layout_dx, mid_y - badge_h * 0.5f);
    }

    if (name_input && !name_input->is_edit_active()) {
        name_input->set_text(draft.name());
        name_input->set_cursor_position(static_cast<int>(draft.name().size()));
    }
    if (desc_input && !desc_input->is_edit_active()) {
        desc_input->set_text(draft.description());
        desc_input->set_cursor_position(static_cast<int>(draft.description().size()));
    }
}

void EditSystem::commit_text_fields() {
    if (name_input) draft.set_name(name_input->get_text());
    if (desc_input) draft.set_description(desc_input->get_text());
}

bool EditSystem::handle_text_field_event(const SDL_Event& ev) {
    TextInput_t *fields[] = { name_input, desc_input };
    for (TextInput_t *field : fields) {
        if (!field) continue;
        const bool was_editing = field->is_edit_active();
        if (field->handle_mouse_event(ev)) {
            if (was_editing && ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_RETURN) {
                commit_text_fields();
                field->set_edit_active(false);
            } else if (was_editing && !field->is_edit_active()) {
                commit_text_fields();
            }
            // Only one field active at a time.
            if (field->is_edit_active()) {
                for (TextInput_t *other : fields) {
                    if (other && other != field && other->is_edit_active()) {
                        commit_text_fields();
                        other->set_edit_active(false);
                    }
                }
            }
            updated = true;
            return true;
        }
        if (was_editing && ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            commit_text_fields();
            field->set_edit_active(false);
            updated = true;
            // Don't return — let the click fall through to the other field / UI.
        }
    }
    return false;
}

void EditSystem::rebuild_ui_from_draft() {
    // Only push field → draft when a field is mid-edit. Otherwise (e.g. right
    // after start_new / start_from_config) empty inputs would wipe draft text.
    if ((name_input && name_input->is_edit_active()) ||
        (desc_input && desc_input->is_edit_active())) {
        commit_text_fields();
    }
    slots_panel->set_name_resolver([this](int slot) { return draft.slot_device_name(slot); });
    slots_panel->refresh_names();
    drives_panel->rebuild(draft.drive_specs());
    platform_con->selected_value(draft.config().platform_id);
    refresh_badge();
    updated = true;
}

void EditSystem::dismiss_card_picker() {
    delete card_picker;
    card_picker = nullptr;
    picking_slot = -1;
}

void EditSystem::show_card_picker(int slot) {
    dismiss_card_picker();
    picking_slot = slot;

    Style_t MS{
        .background_color = 0x5C2A9DFF, // rich purple
        .border_color = 0xFFD166FF,     // warm gold border
        .hover_color = 0x6C3BB0FF,
        .padding = 8,
        .border_width = 3,
    };
    card_picker = new Container_t(&ui_ctx, MS);
    card_picker->set_position(360 + layout_dx, 160 + body_dy);

    constexpr float kBtnW = 320.0f;
    constexpr float kBtnH = 28.0f;
    constexpr float kPanelW = 360.0f;

    auto add_choice = [this, slot](device_id id, const char *label) {
        Button_t *b = new Button_t(&ui_ctx, label, card_picker_btn_style());
        b->size(kBtnW, kBtnH);
        b->on_click([this, slot, id](const SDL_Event&) -> bool {
            draft.set_slot_device(slot, id);
            dismiss_card_picker();
            rebuild_ui_from_draft();
            return true;
        });
        card_picker->add(b);
    };

    add_choice(DEVICE_ID_NONE, "None");
    for (const auto& choice :
         cards_allowed_for_slot(draft.config().platform_id, slot, draft.config().slot_devices)) {
        add_choice(choice.id, choice.display_name);
    }

    // Size panel to fit all choices (one column), then lay out.
    const size_t n = card_picker->count();
    const float pad = static_cast<float>(MS.padding);
    const float cell_h = kBtnH + pad * 2;
    const float panel_h = pad * 2 + n * cell_h + static_cast<float>(MS.border_width) * 2;
    card_picker->size(kPanelW, panel_h);
    card_picker->layout();
    updated = true;
}

void EditSystem::open_premount_dialog(storage_key_t key) {
    pending_mount_key = key;
    static const SDL_DialogFileFilter filters[] = {
        {"Disk Images", "do;po;woz;dsk;hdv;2mg;img"},
        {"All files", "*"},
    };

    // Capture editor in a heap callback that also rebuilds UI.
    struct cb_data {
        EditSystem *editor;
        storage_key_t key;
    };
    auto *data = new cb_data{this, key};
    auto callback = [](void *userdata, const char *const *filelist, int) {
        auto *d = static_cast<cb_data *>(userdata);
        EditSystem *editor = d->editor;
        storage_key_t key = d->key;
        delete d;
        if (!filelist || !filelist[0]) return;
        Paths::set_last_file_dialog_dir(filelist[0]);
        editor->draft.set_mount(key.slot, key.drive, filelist[0]);
        editor->rebuild_ui_from_draft();
    };

#if defined(__EMSCRIPTEN__)
    web_open_file_dialog(callback, data, ".do,.po,.woz,.dsk,.hdv,.2mg,.img");
#else
    const std::string& last_path = Paths::get_last_file_dialog_dir();
    SDL_ShowOpenFileDialog(callback, data, vs->window, filters,
                           sizeof(filters) / sizeof(SDL_DialogFileFilter),
                           last_path.empty() ? nullptr : last_path.c_str(), false);
#endif
}

std::string EditSystem::default_save_path() const {
    if (!draft.path().empty()) {
        return draft.path();
    }
    const std::string& last = Paths::get_last_file_dialog_dir();
    std::string dir = last.empty() ? std::string(".") : Paths::get_directory(last);
    if (dir.empty()) dir = ".";
    std::string filename = draft.name();
    for (char& c : filename) {
        if (c == ' ') c = '_';
    }
    if (filename.empty()) filename = "system";
    return dir + "/" + filename + ".gs2";
}

bool EditSystem::write_draft_to_path(const std::string& path) {
    commit_text_fields();
    std::string error;
    if (!validate_slot_devices(draft.config(), error)) {
        status_text = "Save failed: " + error;
        updated = true;
        return false;
    }
    // Save As (different path) → new machine identity / separate BRAM.
    if (!draft.path().empty() && path != draft.path()) {
        draft.set_id(generate_uuid_v4());
    }
    SystemConfig writer;
    writer.set_from_parts(draft.config(), draft.mounts());
    if (!writer.save(path, error)) {
        status_text = "Save failed: " + error;
        updated = true;
        return false;
    }
    draft.set_path(path);
    status_text = "Saved " + path;
    updated = true;
    return true;
}

void EditSystem::begin_save() {
    // Only .gs2 — with the UTI exported in Info.plist, existing .gs2 files match
    // the allowed type and are not shown grayed (macOS "other file types" look).
    static const SDL_DialogFileFilter filters[] = {
        {"GS2 System Config (.gs2)", "gs2"},
    };

    // Keep default path alive for the async dialog lifetime.
    struct save_dialog_data_t {
        EditSystem *editor;
        std::string default_path;
    };
    auto *cb_data = new save_dialog_data_t{this, default_save_path()};

    auto save_callback = [](void *userdata, const char *const *filelist, int) {
        auto *d = static_cast<save_dialog_data_t *>(userdata);
        EditSystem *editor = d->editor;
        delete d;

        // NULL = error; non-NULL with filelist[0] == NULL = cancel — stay in editor.
        if (!filelist || !filelist[0]) {
            editor->updated = true;
            return;
        }

        std::string path = filelist[0];
        if (path.size() < 4 || path.substr(path.size() - 4) != ".gs2") {
            path += ".gs2";
        }
        Paths::set_last_file_dialog_dir(path.c_str());
        if (editor->write_draft_to_path(path)) {
            editor->result = EDIT_SAVED;
        }
    };

#if defined(__EMSCRIPTEN__)
    // No browser save-dialog helper yet; write to the default path directly.
    if (write_draft_to_path(cb_data->default_path)) {
        result = EDIT_SAVED;
    }
    delete cb_data;
#elif defined(__APPLE__)
    // Use UTI-aware panel so existing .gs2 files are not grayed out.
    (void)filters;
    gs2_show_save_gs2_dialog(save_callback, cb_data, vs->window,
                             cb_data->default_path.c_str());
#else
    SDL_ShowSaveFileDialog(save_callback, cb_data, vs->window, filters,
                           sizeof(filters) / sizeof(SDL_DialogFileFilter),
                           cb_data->default_path.c_str());
#endif
}

bool EditSystem::update() {
    if (name_input) name_input->update();
    if (desc_input) desc_input->update();
    if ((name_input && name_input->needs_redraw()) ||
        (desc_input && desc_input->needs_redraw())) {
        updated = true;
    }
    return updated;
}

void EditSystem::render() {
    if (!updated) return;

    platform_info *plat = get_platform(draft.config().platform_id);
    uint32_t case_color = plat ? (plat->case_color & 0xFFFFFF00) | 0xE0 : 0x808080E0;

    ui_ctx.fill_rect({0, 0, (float)design_width, (float)design_height}, 0x000000FF);
    ui_ctx.fill_rect({20, 30, (float)(design_width - 40), (float)(design_height - 60)}, case_color);

    title_renderer->set_color(0, 0, 0, 0xFF);
    title_renderer->render("Edit System Configuration", design_width / 2, 30, TEXT_ALIGN_CENTER);

    // Match OSD: translucent container fills (e.g. storage 0x00000040) need blend.
    SDL_SetRenderDrawBlendMode(vs->renderer, SDL_BLENDMODE_BLEND);

    badge->render();
    if (name_input) name_input->render();
    if (desc_input) desc_input->render();
    slots_panel->render();
    drives_panel->render();
    speed_con->render();
    display_con->render();
    platform_con->render();
    action_con->render();

    text_renderer->set_color(0, 0, 0, 0xFF);
    text_renderer->render("Slots", 30 + layout_dx, 120 + body_dy, TEXT_ALIGN_LEFT);
    text_renderer->render("Storage (pre-mount)", 400 + layout_dx, 120 + body_dy, TEXT_ALIGN_LEFT);
    text_renderer->render("Speed (not saved)", 30 + layout_dx, 455 + body_dy, TEXT_ALIGN_LEFT);
    text_renderer->render("Display (not saved)", 30 + layout_dx, 550 + body_dy, TEXT_ALIGN_LEFT);
    text_renderer->render("Platform", 400 + layout_dx, 600 + body_dy, TEXT_ALIGN_LEFT);

    if (!status_text.empty()) {
        text_renderer->render(status_text, 30 + layout_dx, design_height - 40, TEXT_ALIGN_LEFT);
    }

    if (card_picker) {
        char title[64];
        snprintf(title, sizeof(title), "Card for slot %d", picking_slot);
        text_renderer->set_color(0xFF, 0xFF, 0xFF, 0xFF);
        text_renderer->render(title, 370 + layout_dx, 140 + body_dy, TEXT_ALIGN_LEFT);
        card_picker->render();
    }

    ui_ctx.color(0x000000FF);
    updated = false;
}

bool EditSystem::event(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT) {
        result = EDIT_QUIT;
        return true;
    }

    if (event.type == SDL_EVENT_WINDOW_RESIZED ||
        event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        event.type == SDL_EVENT_WINDOW_EXPOSED) {
        updated = true;
        return false;
    }

    SDL_Event ev = event;
    SDL_ConvertEventToRenderCoordinates(vs->renderer, &ev);

    if (card_picker) {
        if (card_picker->handle_mouse_event(ev)) {
            updated = true;
            return true;
        }
        if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            dismiss_card_picker();
            updated = true;
            return true;
        }
    }

    if (handle_text_field_event(ev)) {
        return true;
    }

    if (slots_panel->handle_mouse_event(ev)) { updated = true; return true; }
    if (drives_panel->handle_mouse_event(ev)) { updated = true; return true; }
    if (speed_con->handle_mouse_event(ev)) { updated = true; return true; }
    if (display_con->handle_mouse_event(ev)) { updated = true; return true; }
    if (platform_con->handle_mouse_event(ev)) { updated = true; return true; }
    if (action_con->handle_mouse_event(ev)) { updated = true; return true; }

    if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        ev.type == SDL_EVENT_MOUSE_BUTTON_UP ||
        ev.type == SDL_EVENT_MOUSE_MOTION) {
        updated = true;
    }
    return result != EDIT_PENDING;
}
