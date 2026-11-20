/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

namespace ui_roundrect {

struct Cache {
    static constexpr int kFillRadius = 16;
    static constexpr int kCenterStrip = 4;
    static constexpr int kFillSize = kFillRadius * 2 + kCenterStrip; // 36x36
    static constexpr float kBorderOversample = 2.0f;

    SDL_Texture *fill_tex = nullptr;
    // Keyed by (radius_half_px << 16) | thickness_half_px; each texture is
    // 2x supersampled so scale = 0.5f gives exact pixel thickness and smooth AA.
    std::unordered_map<uint32_t, SDL_Texture *> border_tex;

    static void SDLCALL cleanup_on_renderer_destroy(void * /*userdata*/, void *value) {
        // SDL_DestroyRenderer destroys all textures attached to the renderer
        // before tearing down renderer->props, so only free the cache struct here.
        delete static_cast<Cache *>(value);
    }

    static Cache *for_renderer(SDL_Renderer *renderer) {
        if (!renderer) return nullptr;
        SDL_PropertiesID props = SDL_GetRendererProperties(renderer);
        if (props == 0) return nullptr;
        constexpr const char *kPropName = "gs2.ui.roundrect_cache";
        auto *cache = static_cast<Cache *>(SDL_GetPointerProperty(props, kPropName, nullptr));
        if (!cache) {
            cache = new Cache();
            SDL_SetPointerPropertyWithCleanup(props, kPropName, cache,
                                              cleanup_on_renderer_destroy, nullptr);
        }
        return cache;
    }

    static float box_sdf(float px, float py, float half_w, float half_h, float radius) {
        const float dx = std::fabs(px - half_w) - (half_w - radius);
        const float dy = std::fabs(py - half_h) - (half_h - radius);
        const float ox = std::max(dx, 0.0f);
        const float oy = std::max(dy, 0.0f);
        return std::hypot(ox, oy) + std::min(std::max(dx, dy), 0.0f) - radius;
    }

    static SDL_Texture *upload_alpha_mask(SDL_Renderer *renderer, int size,
                                          const std::vector<uint8_t> &rgba) {
        SDL_Texture *tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, size, size);
        if (!tex) return nullptr;
        SDL_UpdateTexture(tex, nullptr, rgba.data(), size * 4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        return tex;
    }

    SDL_Texture *get_fill(SDL_Renderer *renderer) {
        if (fill_tex) return fill_tex;
        const int sz = kFillSize;
        const float half = sz * 0.5f;
        const float r = static_cast<float>(kFillRadius);
        std::vector<uint8_t> pixels(static_cast<size_t>(sz) * sz * 4, 255);
        for (int y = 0; y < sz; ++y) {
            for (int x = 0; x < sz; ++x) {
                const float sd = box_sdf(x + 0.5f, y + 0.5f, half, half, r);
                const float cov = std::clamp(0.5f - sd, 0.0f, 1.0f);
                pixels[(static_cast<size_t>(y) * sz + x) * 4 + 3] =
                    static_cast<uint8_t>(std::lround(cov * 255.0f));
            }
        }
        fill_tex = upload_alpha_mask(renderer, sz, pixels);
        return fill_tex;
    }

    SDL_Texture *get_border(SDL_Renderer *renderer, int r_half_px, int t_half_px, int &out_r_tex, int &out_size) {
        r_half_px = std::clamp(r_half_px, 2, 64);
        t_half_px = std::clamp(t_half_px, 1, r_half_px);
        out_r_tex = r_half_px; // At 2x oversampling, half-pixel units == texels.
        out_size = out_r_tex * 2 + kCenterStrip;

        const uint32_t key = (static_cast<uint32_t>(r_half_px) << 16) |
                             static_cast<uint32_t>(t_half_px);
        auto it = border_tex.find(key);
        if (it != border_tex.end()) return it->second;

        const float half = out_size * 0.5f;
        const float r = static_cast<float>(out_r_tex);
        const float t = static_cast<float>(t_half_px);
        std::vector<uint8_t> pixels(static_cast<size_t>(out_size) * out_size * 4, 255);
        for (int y = 0; y < out_size; ++y) {
            for (int x = 0; x < out_size; ++x) {
                const float sd = box_sdf(x + 0.5f, y + 0.5f, half, half, r);
                const float cov_out = std::clamp(0.5f - sd, 0.0f, 1.0f);
                const float cov_in  = std::clamp(0.5f + (sd + t), 0.0f, 1.0f);
                const float cov = cov_out * cov_in;
                pixels[(static_cast<size_t>(y) * out_size + x) * 4 + 3] =
                    static_cast<uint8_t>(std::lround(cov * 255.0f));
            }
        }
        SDL_Texture *tex = upload_alpha_mask(renderer, out_size, pixels);
        border_tex.emplace(key, tex);
        return tex;
    }
};

inline void fill(SDL_Renderer *renderer, SDL_FRect rect, float radius, uint32_t rgba,
                 bool round_top = true, bool round_bottom = true) {
    if (!renderer || rect.w <= 0.0f || rect.h <= 0.0f || (rgba & 0xFF) == 0) return;

    const float r = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    if (r <= 0.5f || (!round_top && !round_bottom)) {
        SDL_SetRenderDrawColor(renderer, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                               (rgba >> 8) & 0xFF, rgba & 0xFF);
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    Cache *cache = Cache::for_renderer(renderer);
    SDL_Texture *tex = cache ? cache->get_fill(renderer) : nullptr;
    if (!tex) {
        SDL_SetRenderDrawColor(renderer, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                               (rgba >> 8) & 0xFF, rgba & 0xFF);
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    SDL_SetTextureColorMod(tex, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF);
    SDL_SetTextureAlphaMod(tex, rgba & 0xFF);

    constexpr float kR = static_cast<float>(Cache::kFillRadius);
    constexpr float kS = static_cast<float>(Cache::kFillSize);
    constexpr float kHalf = kS * 0.5f;
    const float scale = r / kR;

    SDL_FRect src{0.0f, 0.0f, kS, kS};
    float top_h = kR;
    float bot_h = kR;
    if (round_top && !round_bottom) {
        src = SDL_FRect{0.0f, 0.0f, kS, kHalf};
        bot_h = 0.0f;
    } else if (!round_top && round_bottom) {
        src = SDL_FRect{0.0f, kHalf, kS, kHalf};
        top_h = 0.0f;
    }

    SDL_RenderTexture9Grid(renderer, tex, &src, kR, kR, top_h, bot_h, scale, &rect);
}

inline void stroke(SDL_Renderer *renderer, SDL_FRect rect, float radius, uint32_t rgba,
                   float thickness = 1.0f, bool round_top = true, bool round_bottom = true) {
    if (!renderer || rect.w <= 0.0f || rect.h <= 0.0f || thickness <= 0.0f || (rgba & 0xFF) == 0) {
        return;
    }

    const float max_r = std::min(rect.w, rect.h) * 0.5f;
    const float r = std::clamp(radius, 0.0f, max_r);
    if (r <= 0.5f || (!round_top && !round_bottom)) {
        SDL_SetRenderDrawColor(renderer, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF,
                               (rgba >> 8) & 0xFF, rgba & 0xFF);
        const int it = std::max(1, static_cast<int>(std::lround(thickness)));
        for (int i = 0; i < it; ++i) {
            SDL_FRect br{rect.x + i, rect.y + i, rect.w - 2 * i, rect.h - 2 * i};
            SDL_RenderRect(renderer, &br);
        }
        return;
    }
    if (thickness >= max_r - 0.1f) {
        fill(renderer, rect, r, rgba, round_top, round_bottom);
        return;
    }

    Cache *cache = Cache::for_renderer(renderer);
    if (!cache) return;

    int r_tex = 0;
    int tex_size = 0;
    const int r_half = static_cast<int>(std::lround(r * Cache::kBorderOversample));
    const int t_half = static_cast<int>(std::lround(thickness * Cache::kBorderOversample));
    SDL_Texture *tex = cache->get_border(renderer, r_half, t_half, r_tex, tex_size);
    if (!tex) return;

    SDL_SetTextureColorMod(tex, (rgba >> 24) & 0xFF, (rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF);
    SDL_SetTextureAlphaMod(tex, rgba & 0xFF);

    const float rf = static_cast<float>(r_tex);
    const float sf = static_cast<float>(tex_size);
    const float half = sf * 0.5f;
    const float scale = 1.0f / Cache::kBorderOversample;

    SDL_FRect src{0.0f, 0.0f, sf, sf};
    float top_h = rf;
    float bot_h = rf;
    if (round_top && !round_bottom) {
        src = SDL_FRect{0.0f, 0.0f, sf, half};
        bot_h = 0.0f;
    } else if (!round_top && round_bottom) {
        src = SDL_FRect{0.0f, half, sf, half};
        top_h = 0.0f;
    }

    SDL_RenderTexture9Grid(renderer, tex, &src, rf, rf, top_h, bot_h, scale, &rect);
}

} // namespace ui_roundrect
