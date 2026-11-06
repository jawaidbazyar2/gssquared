#include "display/PhosphorPersist.hpp"
#include "devices/displaypp/RGBA.hpp"

#include <cmath>
#include <cstdio>

static constexpr float FLOOR_STEP = 2.0f / 255.0f;

bool PhosphorPersist::init(SDL_Renderer *r) {
    renderer = r;
    // Float history so slow fades do not stall on 8-bit rounding. SDL treats float
    // textures as linear light unless told otherwise; keep it sRGB like the decoded
    // frame so the fade and floor colors are used as-is and black decays to zero.
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_RGBA64_FLOAT);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_ACCESS_NUMBER, SDL_TEXTUREACCESS_TARGET);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, WIDTH);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, HEIGHT);
    SDL_SetNumberProperty(props, SDL_PROP_TEXTURE_CREATE_COLORSPACE_NUMBER, SDL_COLORSPACE_SRGB);
    history = SDL_CreateTextureWithProperties(renderer, props);
    SDL_DestroyProperties(props);
    if (!history) {
        history = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_TARGET, WIDTH, HEIGHT);
    }
    if (!history) {
        printf("Phosphor persistence: history texture creation failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(history, SDL_BLENDMODE_NONE);

    max_blend = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM,
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_MAXIMUM);
    // dst - src on color; alpha unchanged.
    sub_blend = SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_ADD);
    // A renderer may accept the history texture but not these blend operations.
    // Check once at startup and leave the ordinary monochrome path available.
    SDL_BlendMode saved_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &saved_blend);
    SDL_Texture *saved_target = SDL_GetRenderTarget(renderer);
    const bool supported = SDL_SetTextureBlendMode(history, max_blend) &&
        SDL_SetRenderDrawBlendMode(renderer, sub_blend) && SDL_SetRenderTarget(renderer, history);
    SDL_SetRenderTarget(renderer, saved_target);
    SDL_SetRenderDrawBlendMode(renderer, saved_blend);
    SDL_SetTextureBlendMode(history, SDL_BLENDMODE_NONE);
    if (!supported) {
        printf("Phosphor persistence: required render operations unavailable\n");
        shutdown();
        return false;
    }
    valid = false;
    return true;
}

void PhosphorPersist::shutdown() {
    if (history) SDL_DestroyTexture(history);
    history = nullptr;
    valid = false;
}

SDL_Texture *PhosphorPersist::apply(SDL_Texture *frame, float k) {
    SDL_Texture *saved_target = SDL_GetRenderTarget(renderer);
    Uint8 cr, cg, cb, ca;
    SDL_GetRenderDrawColor(renderer, &cr, &cg, &cb, &ca);
    SDL_BlendMode saved_draw_blend;
    SDL_GetRenderDrawBlendMode(renderer, &saved_draw_blend);

    SDL_SetRenderTarget(renderer, history);

    if (!valid) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);
        valid = true;
    } else {
        // k is the fraction of light kept; the history is gamma-encoded, so scale by k^(1/2.2).
        const float k_gamma = std::pow(k, 1.0f / 2.2f);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 1.0f - k_gamma);
        SDL_RenderFillRect(renderer, nullptr);

        SDL_SetRenderDrawBlendMode(renderer, sub_blend);
        SDL_SetRenderDrawColorFloat(renderer, FLOOR_STEP, FLOOR_STEP, FLOOR_STEP, 1.0f);
        SDL_RenderFillRect(renderer, nullptr);
    }

    SDL_BlendMode saved_frame_blend;
    SDL_ScaleMode saved_frame_scale;
    SDL_GetTextureBlendMode(frame, &saved_frame_blend);
    SDL_GetTextureScaleMode(frame, &saved_frame_scale);
    SDL_SetTextureBlendMode(frame, max_blend);
    SDL_SetTextureScaleMode(frame, SDL_SCALEMODE_NEAREST);
    SDL_RenderTexture(renderer, frame, nullptr, nullptr);
    SDL_SetTextureBlendMode(frame, saved_frame_blend);
    SDL_SetTextureScaleMode(frame, saved_frame_scale);

    SDL_SetRenderTarget(renderer, saved_target);
    SDL_SetRenderDrawBlendMode(renderer, saved_draw_blend);
    SDL_SetRenderDrawColor(renderer, cr, cg, cb, ca);
    return history;
}
