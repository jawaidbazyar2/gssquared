#include "display/PhosphorPersist.hpp"
#include "devices/displaypp/RGBA.hpp"

#include <cstdio>

static constexpr float FLOOR_STEP = 2.0f / 255.0f;

bool PhosphorPersist::init(SDL_Renderer *r) {
    renderer = r;
    // Float history so slow fades do not stall on 8-bit rounding.
    history = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA64_FLOAT, SDL_TEXTUREACCESS_TARGET,
                                WIDTH, HEIGHT);
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
        // history *= k
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColorFloat(renderer, 0.0f, 0.0f, 0.0f, 1.0f - k);
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
