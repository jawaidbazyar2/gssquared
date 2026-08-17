#include "SpeedSelect.hpp"
#include "SelectButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"
#include "computer.hpp"
#include "display/display.hpp"

SpeedSelect_t::SpeedSelect_t(UIContext *ctx, const Style_t& initial_style, computer_t *computer)
    : Container_t(ctx, initial_style), computer(computer), clock(computer ? computer->clock : nullptr) {
    Style_t CB = {
        .background_color = 0x00000000,
        .border_color = 0x000000FF,
        .hover_color = 0x00C0C0FF,
        .padding = 2,
        .border_width = 1,        
    };

    // don't set position yet, we'll do that when we open the submenu.
    size(360, 65);
    add(new SelectButton_t(ctx, MHz1_0Button, CB, CLOCK_1_024MHZ));
    add(new SelectButton_t(ctx, MHz2_8Button, CB, CLOCK_2_8MHZ));
    add(new SelectButton_t(ctx, MHz7_159Button, CB, CLOCK_7_159MHZ));
    add(new SelectButton_t(ctx, MHz14_318Button, CB, CLOCK_14_3MHZ));
    add(new SelectButton_t(ctx, MHzInfinityButton, CB, CLOCK_FREE_RUN));
    set_visible(false);

    // iterate tiles and set onclick for each
    for (size_t i = 0; i < tiles.size(); i++) {
        Tile_t *tile = tiles[i];
        tile->on_click([this,tile](const SDL_Event& event) -> bool {
            this->selected_value(tile->value());
            clock_mode_t mode = (clock_mode_t)tile->value();
            this->clock->set_clock_mode(mode);
            if (mode == CLOCK_FREE_RUN && this->computer) {
                this->computer->begin_ludicrous_calibration();
            }
            if (this->computer) {
                display_state_t *ds = (display_state_t *)this->computer->get_module_state(MODULE_DISPLAY);
                if (ds) display_update_video_scanner(ds);
            }
            this->set_visible(false);
            return true;
        });
    }

    style.hover_color = 0xEE0000FF;
}

// whenever we go from invisible to visible, set the selected value to the current clock mode
void SpeedSelect_t::set_visible(bool visible) {
    Container_t::set_visible(visible);
    if (visible) {
        this->selected_value(this->clock->get_clock_mode());
    }
}

void SpeedSelect_t::render() {
    if (!visible) return;
    Container_t::render();
}

bool SpeedSelect_t::handle_mouse_event(const SDL_Event& event) {
    if (!visible) return false;
    bool consumed = Container_t::handle_mouse_event(event);
    if (consumed && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        return true;
    }
    return false;
}

SpeedSelect_t::~SpeedSelect_t() {
    // deallocate all tiles
    for (size_t i = 0; i < tiles.size(); i++) {
        delete tiles[i];
    }
    tiles.clear();
}
