#include "HoverControls.hpp"
#include "FadeContainer.hpp"
#include "LabeledButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"
#include "SpeedSelect.hpp"
#include "DisplaySelect.hpp"
#include "util/MenuInterface.h"
#include "computer.hpp"

HoverControls_t::HoverControls_t(UIContext *ctx, const Style_t& initial_style, computer_t *computer) : 
    FadeContainer_t(ctx, initial_style) {
    mi = getMenuInterface();

    //hover_controls_con = new FadeContainer_t(&ui_ctx, HUD, 512);
    set_position(10, 100);
    size(65, 500);

    Style_t SB;
    SB.background_color = 0x00000000;
    SB.border_width = 0;
    SB.border_color = 0x000000FF;
    SB.padding = 0;

    {
        LabeledButton *b1 = new LabeledButton(ctx, ResetButton, "", 0);
        b1->size(60, 60);
        b1->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineReset();
            return true;
        });
        add(b1);

        LabeledButton *b3 = new LabeledButton(ctx, GreenDisplayButton, "Capture", 0);
        b3->size(60, 60);
        b3->on_click([this](const SDL_Event& event) -> bool {
            getMenuInterface()->machineCaptureMouse();
            return true;
        });
        add(b3);

        LabeledButton *b2 = new LabeledButton(ctx, GreenDisplayButton, "Debug", 0);
        b2->size(60, 60);
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
        //ncontainers.push_back(hover_controls_con);

        hov_speed = new LabeledButton(ctx, MHz1_0Button, "Speed", 0);
        hov_speed->size(60, 60);
        hov_speed->on_click([this](const SDL_Event& event) -> bool {
            // open the speed submenu container
            if (!hov_speed_con->is_visible()) {            
                // get position of b4
                float x,y;
                hov_speed_con->set_visible(true);
                hov_speed->get_tile_position(x, y);
                hov_speed_con->set_position(x+60, y);
                hov_speed_con->layout();
            } else hov_speed_con->set_visible(false);
        
            return true;
        });
        add(hov_speed);

        hov_display = new LabeledButton(ctx, ColorDisplayButton, "Display", 0);
        hov_display->size(60, 60);
        hov_display->on_click([this](const SDL_Event& event) -> bool {
            // open the speed submenu container
            if (!hov_display_con->is_visible()) {            
                // get position of b4
                float x,y;
                hov_display_con->set_visible(true);
                hov_display->get_tile_position(x, y);
                hov_display_con->set_position(x+60, y);
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

    hov_speed->set_assetID(speed_asset.at(getMenuInterface()->getCurrentSpeed()));
    hov_display->set_assetID(monitor_asset.at(getMenuInterface()->getCurrentMonitor()));
}

HoverControls_t::~HoverControls_t() {

}

