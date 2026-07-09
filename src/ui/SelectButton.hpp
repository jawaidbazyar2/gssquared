#pragma once

#include "Button.hpp"

class SelectButton_t : public Button_t {
public:
    SelectButton_t(UIContext *ctx, const std::string& button_text, const Style_t& style = Style_t(), int64_t value = 0)
        : Button_t(ctx, button_text, style, value) {
    }
    SelectButton_t(UIContext *ctx, int assetID, const Style_t& style = Style_t(), int64_t value = 0)
        : Button_t(ctx, assetID, style, value) {
    }

    void calc_style() override {
        estyle = style;
        // priority: hover color, then active color, then background color
        // Pair each background with a high-contrast text color (used by text SelectButtons).
        if (is_hovering) {
            estyle.background_color = estyle.hover_color;
            estyle.text_color = 0xFFFFFFFF; // white on hover bg
        } else if (active) {
            estyle.background_color = 0x00FF00FF; // same selected green as before (OSD image buttons)
            estyle.text_color = 0x000000FF; // black on bright green
        } else {
            estyle.background_color = 0x1A1A2EFF; // dark navy
            estyle.text_color = 0xFFFFFFFF; // white on navy
        }
    }

};
