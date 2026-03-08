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
        if (is_hovering) { 
            estyle.background_color = estyle.hover_color; 
        } else if (active) {
            estyle.background_color = 0x00FF00FF;
        } else {
            estyle.background_color = 0x000000FF;
        }
    }

};