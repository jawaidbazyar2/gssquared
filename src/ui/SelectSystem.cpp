#include "SelectSystem.hpp"
#include "Container.hpp"
#include "systemconfig.hpp"
#include "platforms.hpp"
#include "Style.hpp"
#include "util/TextRenderer.hpp"
#include "util/SystemConfig.hpp"
#include "util/SystemSettings.hpp"
#include "AssetAtlas.hpp"
#include "SystemButton.hpp"
#include "Button.hpp"

#include <iostream>

SelectSystem::SelectSystem(video_system_t *vs, AssetAtlas_t *aa)
    : vs(vs), aa(aa) {

// create container with tiles for each system.
// be cheap right now and do a text button.
    Style_t CS = {
        .background_color = 0x000000FF,
        .padding = 25,
        .border_width = 0,
    };

    Style_t BS = {
        .background_color = 0xBFBB98FF,
        .border_color = 0xBFBB9840,
        .hover_color = 0x008C4AFF,
        .padding = 15,
        .border_width = 1,
        .text_color = 0xFFFFFFFF
    };

    int num_configs = NUM_SYSTEM_CONFIGS;

    text_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 24);
    // ~33% smaller than the title/hover font for custom tile name labels.
    name_renderer = new TextRenderer(vs->renderer, "fonts/OpenSans-Regular.ttf", 16.0f);
    ui_ctx = { vs->renderer, vs->window, text_renderer, nullptr, aa };

    container = new Container_t(&ui_ctx, CS);

    // Lay out the selector against a fixed "design" resolution rather than the
    // live window/canvas size. At render time we map this design space onto the
    // actual output with SDL_SetRenderLogicalPresentation(... LETTERBOX), so the
    // aspect ratio is always preserved and all content stays on-screen no matter
    // how big the window/canvas is (important on the web, where the canvas
    // drawable size doesn't necessarily match the window size at startup).
    design_width = vs->window_width  > 0 ? vs->window_width  : 1288;
    design_height = vs->window_height > 0 ? vs->window_height : 928;
    window_width = design_width;
    window_height = design_height;

    container->size(1024, 768);
    container->set_position((design_width - 1024) / 2, (design_height - 768) / 2);

    // Render the selector through a fixed design-resolution logical presentation.
    // LETTERBOX keeps the aspect ratio correct and all content on-screen no
    // matter the real window/canvas size. We set it for the whole lifetime of
    // the selector (not just inside render()) because SDL fills the letterbox
    // bars with black during SDL_RenderPresent() ONLY while this mode is active
    // — and present() happens in the app's iterate callback, after render()
    // returns. It is reset back to DISABLED in transition_to_emulation() so the
    // emulator/OSD/debugger don't inherit our transform.
    SDL_SetRenderLogicalPresentation(vs->renderer, design_width, design_height,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    selected_system = SELECT_PENDING;

    // add a text button for each system.
    for (int i = 0; i < num_configs; i++) {
        platform_info* platform = get_platform(BuiltinSystemConfigs[i].platform_id);
        SystemButton *button = new SystemButton(&ui_ctx, &BuiltinSystemConfigs[i], platform->image_id, BS);
        button->size(200, 200);
        button->position_content(CP_CENTER, CP_CENTER);
        button->style.background_color = platform->case_color;

        button->on_click([this,i](const SDL_Event& event) -> bool {
            selected_system = i;
            return true;
        });
        container->add(button);
    }

    Style_t ActionStyle = {
        .background_color = 0x404040FF,
        .border_color = 0xFFFFFFFF,
        .hover_color = 0x008C4AFF,
        .padding = 10,
        .border_width = 1,
        .text_color = 0xFFFFFFFF,
    };
    Button_t *new_btn = new Button_t(&ui_ctx, "+", ActionStyle);
    new_btn->size(200, 200);
    new_btn->on_click([this](const SDL_Event&) -> bool {
        selected_system = SELECT_NEW;
        return true;
    });
    container->add(new_btn);

    Button_t *edit_btn = new Button_t(&ui_ctx, "Edit...", ActionStyle);
    edit_btn->size(200, 200);
    edit_btn->on_click([this](const SDL_Event&) -> bool {
        selected_system = SELECT_OPEN_EDIT;
        return true;
    });
    container->add(edit_btn);

    // Recent custom configs: MRU + up to 4 by usage score.
    for (const auto& entry : SystemSettings::instance().display_entries()) {
        auto loaded = std::make_unique<SystemConfig>();
        std::string error;
        if (!loaded->load(entry.path, error)) {
            std::cerr << "SelectSystem: skip recent config '" << entry.path
                      << "': " << error << std::endl;
            continue;
        }

        platform_info* platform = get_platform(loaded->config().platform_id);
        if (platform == nullptr) {
            continue;
        }

        recent_loaded_.push_back(std::move(loaded));
        recent_paths_.push_back(entry.path);
        const int recent_index = static_cast<int>(recent_paths_.size()) - 1;
        const SystemConfig_t* cfg = &recent_loaded_.back()->config();

        SystemButton *button = new SystemButton(&ui_ctx, cfg, platform->image_id, BS, true, name_renderer);
        button->size(200, 200);
        button->position_content(CP_CENTER, CP_CENTER);
        button->style.background_color = platform->case_color;
        button->on_click([this, recent_index](const SDL_Event&) -> bool {
            selected_system = SELECT_RECENT_BASE + recent_index;
            return true;
        });
        container->add(button);
    }

    container->layout();

    updated = true;
}

SelectSystem::~SelectSystem() {
    delete container;
    delete text_renderer;
    delete name_renderer;
}

bool SelectSystem::event(const SDL_Event &event) {
    if (event.type == SDL_EVENT_QUIT) {
        selected_system = SELECT_QUIT;
        return true;
    }

    // The window/canvas was resized (or the browser changed the canvas
    // drawable size). Our layout is in fixed design coordinates, so nothing
    // needs to move, but we must repaint: the renderer only redraws on
    // updated==true, and on the web a resize blanks the WebGL backing buffer.
    if (event.type == SDL_EVENT_WINDOW_RESIZED ||
        event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
        event.type == SDL_EVENT_WINDOW_EXPOSED) {
        updated = true;
        return false;
    }

    // Mouse coordinates arrive in window space; convert them into the renderer's
    // logical (design) space so hit-testing matches what we draw. The renderer's
    // LETTERBOX logical presentation is left active for the selector's lifetime
    // (see constructor), so the conversion uses the same mapping as rendering.
    SDL_Event ev = event;
    SDL_ConvertEventToRenderCoordinates(vs->renderer, &ev);

    container->handle_mouse_event(ev);
    if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ||
        (event.type == SDL_EVENT_MOUSE_BUTTON_UP) || 
        (event.type == SDL_EVENT_MOUSE_MOTION)) {
        updated = true;
    }
    return (selected_system >= 0) ||
           (selected_system == SELECT_NEW) ||
           (selected_system == SELECT_OPEN_EDIT);
}

int SelectSystem::get_selected_system() {
    return selected_system;
}

const std::string& SelectSystem::get_recent_path(int selection_id) const {
    static const std::string empty;
    const int index = selection_id - SELECT_RECENT_BASE;
    if (index < 0 || index >= static_cast<int>(recent_paths_.size())) {
        return empty;
    }
    return recent_paths_[static_cast<size_t>(index)];
}

bool SelectSystem::update() {
    return updated;
}

void SelectSystem::render() {
    if (updated) {
        // The LETTERBOX logical presentation is already active (set in the
        // constructor). Draw everything in design coordinates; SDL maps it onto
        // the real output and fills the letterbox bars with black at present().
        container->render();

        text_renderer->set_color(255,255,255,255);
        text_renderer->render("Choose your retro experience", (design_width / 2), 50, TEXT_ALIGN_CENTER);

        ui_ctx.color(0x000000FF); // set back to 0. Someone isn't correctly setting color elsewhere..
        updated = false;
    }
}
