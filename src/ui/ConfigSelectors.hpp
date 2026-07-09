/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include "Container.hpp"
#include "SelectButton.hpp"
#include "Style.hpp"
#include "UIContext.hpp"

/** Shared button style used by OSD speed/monitor rows. */
Style_t config_selector_button_style();

/**
 * Populate a container with CPU-speed SelectButtons (values = clock_mode_t).
 * Does not attach click handlers — caller wires them.
 * Returns pointers to the five buttons in order: 1.0, 2.8, 7.1, 14.3, free-run.
 */
void populate_speed_selector(Container_t *container, UIContext *ctx, const Style_t& button_style,
                             SelectButton_t **out_btns = nullptr);

/**
 * Populate a container with monitor-type SelectButtons (values = monitor enums).
 * Does not attach click handlers — caller wires them.
 */
void populate_display_selector(Container_t *container, UIContext *ctx, const Style_t& button_style);
