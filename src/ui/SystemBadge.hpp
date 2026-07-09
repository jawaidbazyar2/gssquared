/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <string>

#include "Button.hpp"
#include "Style.hpp"
#include "UIContext.hpp"

/**
 * Platform badge image plus name/description text (control panel title strip).
 */
class SystemBadge_t {
protected:
    UIContext *ctx = nullptr;
    Button_t *badge = nullptr;
    std::string name;
    std::string description;
    float text_x = 220.0f;
    float name_y = 70.0f;
    float desc_y = 90.0f;
    bool draw_name = true;
    bool draw_description = true;

public:
    SystemBadge_t(UIContext *ctx, int image_id, const Style_t& style,
                  const std::string& name, const std::string& description);
    ~SystemBadge_t();

    void set_position(float x, float y);
    void set_image_id(int image_id);
    void set_text(const std::string& name, const std::string& description);
    /** When false, name/description text is omitted (caller supplies editable fields). */
    void set_draw_name(bool draw) { draw_name = draw; }
    void set_draw_description(bool draw) { draw_description = draw; }
    void render();

    float get_text_x() const { return text_x; }
    float get_name_y() const { return name_y; }
    float get_desc_y() const { return desc_y; }

    Button_t *get_badge() const { return badge; }
};
