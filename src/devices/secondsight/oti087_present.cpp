/*
 *   Upload Second Sight's Oak image to a texture in SDL, the host graphics
 *   library. VRAM means the card's video memory. SecondSightDisplay
 *   calls upload_frame() on the main thread after SSCard has stopped its worker.
 *   The Oak renderer supplies opaque pixels; SDL receives only rows that changed.
 *   A new texture or changed geometry forces a full copy.
 *
 *   The card owns its pixel cache; the caller owns the texture. ARGB8888 means
 *   four eight-bit components: alpha, red, green and blue. XRGB8888 has the same
 *   layout but ignores alpha. No chip timing is changed by presentation.
 *   The source for raster/color behavior is the renderer's Oak OTI-087 databook
 *   and Brooktree Bt481A L481A_C references; texture caching is host policy.
 */

#include "oti087.hpp"
#include "videosystem.hpp"
#include "ss_crt.hpp" // Host monitor scan, after native pixels are complete.

// Upload changed rows after rasterization; a new SDL texture requires a full
// copy even if the emulated VRAM has not changed.
bool OTI087::upload_frame(SDL_Texture *texture, bool full, SecondSightCrt *crt, SDL_Texture *odd_texture) {
    int const width = timing.width, height = timing.height;
    // The texture contains the monitor window, including optional scanlines.
    // full forces upload even when the cached image is unchanged.
    frame_pixels.resize(width * height);
    render_frame({frame_pixels.data(), width, height, width});
    if (full || width != present_width || height != present_height) {
        present_top = 0;
        present_bottom = height - 1;
    }
    // Native ARGB8888 pixels are opaque; XRGB8888 needs no conversion. Upload damage
    // directly, including rows refreshed by a debug snapshot between frames.
    int const top = std::max(0, present_top), bottom = std::min(height - 1, present_bottom);
    // The visible scan includes overscan, with optional retained field textures. Source
    // pixels stay untouched; failed SDL uploads retain damage for a retry.
    bool direct = crt && !crt->sink && !full && crt->direct_upload(width, height);
    std::vector<SecondSightCrt::BorderChange> border_changes;
    if (crt && !display_changes.empty()) {
        // IBM VGA/XGA Technical Reference, May 1992, p. 2-94: the resolved
        // overscan color matters here. Other palette/register writes already
        // reached the active raster and need not repaint an unchanged border.
        const uint32_t border = border_color().value;
        for (const auto &change : display_changes) {
            const auto &state = change.registers;
            if (state.dac.pixel_select(state.attribute.data[0x11]).value != border) {
                // A later change can restore the original color; retain every
                // pre-write boundary once any saved color differs from live.
                border_changes.reserve(display_changes.size());
                for (const auto &entry : display_changes)
                    border_changes.push_back({entry.position,
                        entry.registers.dac.pixel_select(entry.registers.attribute.data[0x11]).value});
                direct = false;
                break;
            }
        }
    }
    if (direct) {
        // The scan texture already contains the border and blanking. Upload
        // changed active pixels at their exact scan coordinates with the native
        // pitch; the usual full-width region needs no SDL row repacking either.
        // Overscan changes keep the compositor path. Failed uploads leave
        // source damage pending, so the complete set is retried next time.
        for (const auto &region : crt->active_regions()) {
            const int first = std::max(top, region.source_y);
            const int last = std::min(bottom + 1, region.source_y + region.height);
            if (last <= first) continue;
            SDL_Rect damage = {region.left, region.top + first - region.source_y, region.width, last - first};
            if (!SDL_UpdateTexture(texture, &damage, frame_pixels.data() + size_t(first) * width + region.source_x,
                width * sizeof(uint32_t))) return false;
            crt->source_uploaded();
        }
    } else if (crt) {
        // A renderer switch or recreated texture cannot borrow its old GPU
        // contents, including rows previously uploaded without a staging copy.
        if (full) crt->invalidate();
        crt->update(frame_pixels.data(), width, height, top, bottom,
                    border_changes.empty() ? nullptr : &border_changes);
        if (full) crt->full_upload();
        if (crt->bottom() >= crt->top() && crt->sink) {
            if (!crt->sink(crt->pixels(), crt->top(), crt->bottom())) return false;
        } else if (crt->bottom() >= crt->top()) {
            SDL_Rect damage = {0, crt->top(), crt->width(), crt->bottom() - crt->top() + 1};
            for (int field = 0; field < (crt->fields() ? 2 : 1); ++field) {
                if (!SDL_UpdateTexture(field ? odd_texture : texture, &damage,
                    crt->pixels(field) + size_t(damage.y) * crt->width(), crt->width() * sizeof(uint32_t))) return false;
            }
        }
        crt->uploaded();
    } else if (bottom >= top) {
        SDL_Rect damage = {0, top, width, bottom - top + 1};
        if (!SDL_UpdateTexture(texture, &damage, frame_pixels.data() + size_t(top) * width,
                               width * sizeof(uint32_t))) return false;
    }
    // Clear the pending upload range only after SDL accepts it. On failure
    // the early return above leaves those rows pending for another attempt.
    present_top = 0;
    present_bottom = -1;
    present_width = width;
    present_height = height;
    return true;
}
