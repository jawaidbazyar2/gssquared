#include "HoverControls.hpp"
#include "FadeContainer.hpp"
#include "FaceButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"
#include "SpeedSelect.hpp"
#include "DisplaySelect.hpp"
#include "util/MenuInterface.h"
#include "computer.hpp"

namespace {

uint32_t monitor_accent(int monitor) {
    switch (monitor) {
        case MONITOR_MONO_GREEN: return 0x00FF4AFF;
        case MONITOR_MONO_AMBER: return 0xFFBF00FF;
        case MONITOR_MONO_WHITE: return 0xF2F2F2FF;
        case MONITOR_GS_RGB:     return 0xFF5A8AFF;
        default:                 return 0x4C8CFFFF;
    }
}

FaceButton *make_key(UIContext *ctx, const char *legend, uint32_t accent) {
    FaceButton *button = new FaceButton(ctx, legend);
    button->set_accent(accent);
    button->size(60, 60);
    return button;
}

}  // namespace

HoverControls_t::HoverControls_t(UIContext *ctx, const Style_t& initial_style, computer_t *computer) :
    FadeContainer_t(ctx, initial_style) {
    mi = getMenuInterface();

    set_position(10, 100);
    size(65, 500);

    Style_t SB;
    SB.background_color = 0x00000000;
    SB.border_width = 0;
    SB.border_color = 0x000000FF;
    SB.padding = 0;

    {
        FaceButton *b1 = make_key(ctx, "RESET", 0xE0A040FF);
        b1->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineReset();
            return true;
        });
        add(b1);

        FaceButton *b3 = make_key(ctx, "Capture", 0x3DDC78FF);
        b3->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineCaptureMouse();
            return true;
        });
        add(b3);

        FaceButton *b2 = make_key(ctx, "Debug", 0x49B8FFFF);
        b2->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->openDebugWindow();
            return true;
        });
        add(b2);

        hov_speed_con = new SpeedSelect_t(ctx, SB, computer);
        hov_speed_con->set_visible(false);

        hov_display_con = new DisplaySelect(ctx, SB);
        hov_display_con->set_visible(false);

        add(hov_speed_con);
        add(hov_display_con);

        hov_speed = make_key(ctx, "1.0", 0x5C78FFFF);
        hov_speed->on_click([this](const SDL_Event& event) -> bool {
            if (!hov_speed_con->is_visible()) {
                float x, y, bw, bh;
                hov_speed_con->set_visible(true);
                hov_speed->get_tile_position(x, y);
                hov_speed->get_tile_size(&bw, &bh);
                hov_speed_con->set_position(x + bw, y);
                hov_speed_con->layout();
            } else hov_speed_con->set_visible(false);

            return true;
        });
        add(hov_speed);

        hov_display = make_key(ctx, "Display", 0x4C8CFFFF);
        hov_display->on_click([this](const SDL_Event& event) -> bool {
            if (!hov_display_con->is_visible()) {
                float x, y, bw, bh;
                hov_display_con->set_visible(true);
                hov_display->get_tile_position(x, y);
                hov_display->get_tile_size(&bw, &bh);
                hov_display_con->set_position(x + bw, y);
                hov_display_con->layout();
            } else hov_display_con->set_visible(false);

            return true;
        });
        add(hov_display);

        layout();
    }
}

void HoverControls_t::update() {
    if (mi->isMouseCaptured()) {
        set_visible(false);
    } else {
        set_visible(true);
    }

    if (frameCount == 0) { // if we're not visible, hide the submenus
        hov_speed_con->set_visible(false);
        hov_display_con->set_visible(false);
    }

    const int speed = getMenuInterface()->getCurrentSpeed();
    if (speed == CLOCK_FREE_RUN) {
        hov_speed->show_image(MHzInfinityButton);
    } else {
        hov_speed->show_text(speed_button_label(speed));
    }
    hov_display->set_accent(monitor_accent(getMenuInterface()->getCurrentMonitor()));
}

HoverControls_t::~HoverControls_t() {

}
