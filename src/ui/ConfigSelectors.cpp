/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "ConfigSelectors.hpp"

#include "FaceButton.hpp"
#include "MainAtlas.hpp"
#include "NClock.hpp"
#include "util/MenuInterface.h"

Style_t config_selector_button_style() {
    return Style_t{
        .background_color = 0x00000000,
        .border_color = 0x000000FF,
        .hover_color = 0x00C0C0FF,
        .padding = 2,
        .border_width = 1,
    };
}

void populate_speed_selector(Container_t *container, UIContext *ctx, const Style_t& button_style,
                             SelectButton_t **out_btns) {
    const clock_mode_t modes[4] = {
        CLOCK_1_024MHZ, CLOCK_2_8MHZ, CLOCK_7_159MHZ, CLOCK_14_3MHZ,
    };
    SelectButton_t *btns[5] = {};
    for (int i = 0; i < 4; i++) {
        FaceButton *face = new FaceButton(ctx, speed_button_label(modes[i]), button_style, modes[i]);
        face->set_accent(0x5C78FFFF);
        face->size(56, 56);
        btns[i] = face;
    }
    btns[4] = new SelectButton_t(ctx, MHzInfinityButton, button_style, CLOCK_FREE_RUN);
    for (int i = 0; i < 5; i++) {
        container->add(btns[i]);
        if (out_btns) out_btns[i] = btns[i];
    }
    container->layout();
}

void populate_display_selector(Container_t *container, UIContext *ctx, const Style_t& button_style) {
    container->add(new SelectButton_t(ctx, ColorDisplayButton, button_style, MONITOR_COMPOSITE));
    container->add(new SelectButton_t(ctx, RGBDisplayButton, button_style, MONITOR_GS_RGB));
    container->add(new SelectButton_t(ctx, GreenDisplayButton, button_style, MONITOR_MONO_GREEN));
    container->add(new SelectButton_t(ctx, AmberDisplayButton, button_style, MONITOR_MONO_AMBER));
    container->add(new SelectButton_t(ctx, WhiteDisplayButton, button_style, MONITOR_MONO_WHITE));
    container->layout();
}
