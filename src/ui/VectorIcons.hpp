/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <cmath>
#include <SDL3/SDL.h>

#include "UIContext.hpp"

namespace ui_icons {

/** Draw a crisp line segment of arbitrary thickness using SDL_RenderGeometry. */
inline void stroke_segment(UIContext *ctx, float x1, float y1, float x2, float y2,
                           float thickness, uint32_t rgba) {
    if (!ctx || !ctx->renderer) return;
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float len = std::hypot(dx, dy);
    if (len < 0.001f) return;
    const float nx = (-dy / len) * (thickness * 0.5f);
    const float ny = (dx / len) * (thickness * 0.5f);
    const SDL_FColor fc = UIContext::rgba_to_fcolor(rgba);
    const SDL_Vertex verts[4] = {
        {{x1 + nx, y1 + ny}, fc, {0.0f, 0.0f}},
        {{x2 + nx, y2 + ny}, fc, {0.0f, 0.0f}},
        {{x2 - nx, y2 - ny}, fc, {0.0f, 0.0f}},
        {{x1 - nx, y1 - ny}, fc, {0.0f, 0.0f}},
    };
    const int idx[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(ctx->renderer, nullptr, verts, 4, idx, 6);
}

/** Draw a flat modern pencil glyph centered inside `box` (normalized 12x12 coords). */
inline void draw_pencil(UIContext *ctx, SDL_FRect box, uint32_t rgba) {
    if (!ctx || !ctx->renderer) return;
    const float s = std::min(box.w, box.h);
    const float ox = box.x + (box.w - s) * 0.5f;
    const float oy = box.y + (box.h - s) * 0.5f;
    auto px = [&](float u) { return ox + (u / 12.0f) * s; };
    auto py = [&](float v) { return oy + (v / 12.0f) * s; };
    const float t = std::max(1.25f, s * 0.115f);

    // Outline of diagonal pencil: tip at (1.8, 10.2), top-right eraser at (8.4, 1.6)-(10.4, 3.6)
    const float p[5][2] = {
        {8.3f, 1.7f},
        {10.3f, 3.7f},
        {4.3f, 9.7f},
        {1.7f, 10.3f},
        {2.3f, 7.7f},
    };
    for (int i = 0; i < 5; ++i) {
        const int j = (i + 1) % 5;
        stroke_segment(ctx, px(p[i][0]), py(p[i][1]), px(p[j][0]), py(p[j][1]), t, rgba);
    }
    // Eraser collar line
    stroke_segment(ctx, px(6.8f), py(3.2f), px(8.8f), py(5.2f), t * 0.9f, rgba);
}

/** Draw a compact X close glyph centered inside `box`. */
inline void draw_close(UIContext *ctx, SDL_FRect box, uint32_t rgba) {
    if (!ctx || !ctx->renderer) return;
    const float s = std::min(box.w, box.h);
    const float ox = box.x + (box.w - s) * 0.5f;
    const float oy = box.y + (box.h - s) * 0.5f;
    const float m = s * 0.24f;
    const float t = std::max(1.35f, s * 0.125f);
    stroke_segment(ctx, ox + m, oy + m, ox + s - m, oy + s - m, t, rgba);
    stroke_segment(ctx, ox + s - m, oy + m, ox + m, oy + s - m, t, rgba);
}

/** Draw a compact left back chevron `<` centered inside `box`. */
inline void draw_back(UIContext *ctx, SDL_FRect box, uint32_t rgba) {
    if (!ctx || !ctx->renderer) return;
    const float s = std::min(box.w, box.h);
    const float ox = box.x + (box.w - s) * 0.5f;
    const float oy = box.y + (box.h - s) * 0.5f;
    const float t = std::max(1.35f, s * 0.125f);
    const float x_right = ox + s * 0.65f;
    const float x_left = ox + s * 0.34f;
    const float y_mid = oy + s * 0.50f;
    stroke_segment(ctx, x_right, oy + s * 0.20f, x_left, y_mid, t, rgba);
    stroke_segment(ctx, x_left, y_mid, x_right, oy + s * 0.80f, t, rgba);
}

/** Draw a compact checkmark centered inside `box`. */
inline void draw_check(UIContext *ctx, SDL_FRect box, uint32_t rgba) {
    if (!ctx || !ctx->renderer) return;
    const float s = std::min(box.w, box.h);
    const float ox = box.x + (box.w - s) * 0.5f;
    const float oy = box.y + (box.h - s) * 0.5f;
    const float t = std::max(1.4f, s * 0.13f);
    const float x0 = ox + s * 0.18f;
    const float y0 = oy + s * 0.54f;
    const float x1 = ox + s * 0.42f;
    const float y1 = oy + s * 0.78f;
    const float x2 = ox + s * 0.84f;
    const float y2 = oy + s * 0.26f;
    stroke_segment(ctx, x0, y0, x1, y1, t, rgba);
    stroke_segment(ctx, x1, y1, x2, y2, t, rgba);
}

} // namespace ui_icons
