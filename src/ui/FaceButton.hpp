/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include "SelectButton.hpp"

/**
 * Drawn key face with the ordinary centered button label on top.
 * Ludicrous / infinite keeps the atlas image instead of a numeric legend.
 */
class FaceButton : public SelectButton_t {
public:
    FaceButton(UIContext *ctx, const std::string &legend, const Style_t &style = Style_t(), int64_t value = 0);

    void set_accent(uint32_t rgba) { accent = rgba; }
    void show_text(const std::string &legend);
    void show_image(int asset_id);

    void calc_style() override;
    void render() override;

private:
    uint32_t accent = 0x5C78FFFF;
    void draw_face();
};

/** Short legend for a numeric clock mode ("1.0", "2.8", "7.1", "14.3"). */
const char *speed_button_label(int clock_mode);
