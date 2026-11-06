/*
 *   Present the Second Sight monitor using pixels from its Oak OTI-087X
 *   video controller. The card has its own screen, even when the Apple IIgs
 *   built-in display shows something different. SSCard offers a stopped scan;
 *   video_system_t then calls this adapter to upload and draw it.
 *
 *   SDL is the host window/graphics library. The application owns this
 *   adapter and all SDL windows/textures. All methods run on the main thread;
 *   render owns stopped Oak state before reading it, or reuses its last texture.
 *   Native builds can use two windows; browser builds place both monitors on
 *   one canvas. Input from either monitor feeds the same emulated Apple.
 *   Source: Second Sight manual, monitor installation; Oak OTI-087 databook
 *   pp. 39-40 describes the screen timing extensions. Window layout and input mapping
 *   are application choices, not card registers.
 */

#include "ss_display.hpp"
#include "ss_monitor.hpp"
#include "ss_card.hpp"
#include "oti087.hpp"
#include "computer.hpp"
#include "videosystem.hpp"
#include "util/SystemSettings.hpp"
#include <algorithm>

// Attach card rendering and input routing to the existing video system.
SecondSightDisplay::SecondSightDisplay(computer_t *computer, SSCard &card)
    : computer(computer), card(card), video(card.video()) {
    computer->video_system->additional_display = [this]() { render(); };
    computer->video_system->additional_display_event = [this](const SDL_Event &event) {
        return handle_event(event);
    };
}

// Detach host callbacks before releasing textures and the optional monitor window.
SecondSightDisplay::~SecondSightDisplay() {
    auto *vs = computer->video_system;
    vs->additional_window = nullptr;
    vs->additional_display = {};
    vs->additional_display_event = {};
    // A screenshot can borrow either retained field of the main card picture.
    if (vs->last_texture == main_texture.value || vs->last_texture == main_texture.odd) vs->last_texture = nullptr;
    if (main_texture.value) SDL_DestroyTexture(main_texture.value);
    if (window_texture.value) SDL_DestroyTexture(window_texture.value);
    // Field textures belong to their respective renderer, like the main image.
    if (main_texture.odd) SDL_DestroyTexture(main_texture.odd);
    if (window_texture.odd) SDL_DestroyTexture(window_texture.odd);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
}

// Create the native card monitor lazily. Browsers share the main canvas
// because an SDL secondary window is not a separate browser monitor.
bool SecondSightDisplay::open_window() {
#if !defined(__EMSCRIPTEN__)
    // Do not retry a failed window allocation every frame. Keep the machine
    // usable with its existing window and report the first SDL error.
    if (window_failed) return false;
    if (window) return true;
    const auto title = gs2_window_title("GSSquared - Second Sight monitor");
    window = SDL_CreateWindow(title.c_str(), 640, 480,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);
    if (window) renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        window_failed = true;
        fprintf(stderr, "Second Sight monitor: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        window = nullptr;
        return false;
    }
    computer->video_system->additional_window = window;
    int x, y, w, h;
    SDL_GetWindowPosition(computer->video_system->window, &x, &y);
    SDL_GetWindowSize(computer->video_system->window, &w, &h);
    SDL_Rect bounds;
    // Place the new monitor beside the Apple window and clamp it to the
    // usable desktop, so its title bar stays reachable on small displays.
    if (SDL_GetDisplayUsableBounds(SDL_GetDisplayForWindow(computer->video_system->window), &bounds)) {
        x = std::max(bounds.x, std::min(x + w, bounds.x + bounds.w - 640));
        y = std::max(bounds.y, std::min(y, bounds.y + bounds.h - 480));
        SDL_SetWindowPosition(window, x, y);
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    return true;
#else
    return false;
#endif
}

// Recreate the texture when Oak changes geometry; force a full upload when
// switching renderers because each texture has its own retained pixels.
// refresh requires ownership of stopped Oak state. Otherwise return the last
// shown texture without reading the card. Null means no drawable texture.
SDL_Texture *SecondSightDisplay::prepare_texture(SDL_Renderer *dest, Texture &texture, bool refresh) {
    // Retained SDL textures are host-owned. Reusing one must not inspect Oak,
    // including its dimensions, current field or dirty-line bookkeeping.
    if (!refresh) return texture.shown;
    texture.shown = nullptr;
    // dest owns this texture; its dimensions come from the programmed card
    // raster. Reject incomplete or excessive modes before allocating pixels.
    const auto &timing = video.display_timing();
    texture.timing = timing;
    if (timing.width < 1 || timing.height < 1 || timing.width > OTI087::MAX_FRAME_DIMENSION || timing.height > OTI087::MAX_FRAME_DIMENSION)
        return nullptr;
    bool full = last_renderer != dest;
    // IBM VGA/XGA CRTC blanking excludes retrace, while attribute $11 colors
    // every visible overscan sample. Read stored state after synchronization;
    // neither monitor sense nor native image dimensions select a cosmetic mode.
    const auto &crtc = video.crtc_state();
    const int hs = crtc.horz_blank_start;
    const int hn = (crtc.horz_blank_end - hs) & 63;
    const int vs = crtc.vert_blank_start;
    const int vn = (crtc.vert_blank_end - crtc.vert_blank_start) & 255;
    const int rows = timing.height / (crtc.vert_disp_end + 1);
    // Center the monitor window on the active display using the user's
    // position controls.
    const double line_hz = 1.0 / timing.line_period();
    const int fields = timing.interlaced ? 2 : 1;
    const bool vga_monitor = video.vga_monitor();
    SecondSightMonitorWindow window = ss_monitor::card_monitor(vga_monitor, line_hz, fields / timing.frame_period(),
        1.0 / timing.pixel_period(), timing.total_width, timing.total_height, fields,
        video.misc_output() & 0x40, video.misc_output() & 0x80);
    if (!window.locked) {
        // No picture, as on a monitor that cannot lock.
        window.width = timing.width;
        window.height = timing.height;
    }
    // A monitor locks to timing that holds for a whole frame. The states that
    // pass while a mode is programmed never reach the screen.
    if (window.locked != lock_candidate) {
        lock_candidate = window.locked;
        lock_since = video.frame_number();
    }
    if (lock_candidate == out_of_range && video.frame_number() >= lock_since + 2) {
        out_of_range = !lock_candidate;
        if (out_of_range)
            fprintf(stderr, "Second Sight %s monitor: out of range (%.2f kHz, %.2f Hz)\n",
                vga_monitor ? "VGA" : "AppleColor RGB", line_hz / 1000.0,
                fields / timing.frame_period());
    }
    const int window_width = std::clamp(window.width, 1, timing.total_width);
    const int window_height = std::clamp(window.height, 1, timing.total_height);
    const int left = ss_monitor::floor_half(window_width - timing.width);
    const int top = ss_monitor::floor_half(window_height - timing.height);
    const int hfirst = int(hs * timing.total_width / timing.line_characters);
    const int hlength = std::min(timing.total_width, int((hn ? hn : 64) * timing.total_width / timing.line_characters));
    const int vfirst = vs * rows, vlength = std::min(timing.total_height, (vn ? vn : 256) * rows);
    const SecondSightCrt::Axis horizontal{timing.total_width, (timing.total_width - left) % timing.total_width,
        window_width, hfirst % timing.total_width, hlength};
    const SecondSightCrt::Axis vertical{timing.total_height, (timing.total_height - top) % timing.total_height,
        window_height, vfirst % timing.total_height, vlength};
    const bool filtered = computer->video_system->get_crt_shader_enabled();
    const int phase = timing.interlaced ? video.odd_field_sync_phase() : 0;
    const bool odd = timing.interlaced && video.vpos() >= timing.total_height / 2;
    texture.crt.configure(horizontal, vertical, filtered, timing.interlaced, video.border_color().value, !out_of_range,
        phase, int(timing.total_width), odd, timing.interlaced);
    const int width = texture.crt.width(), height = texture.crt.height();
    if (width < 1 || height < 1 || width > OTI087::MAX_FRAME_DIMENSION * 2 || height > OTI087::MAX_FRAME_DIMENSION * 2)
        return nullptr;
    if (!texture.value || texture.width != width || texture.height != height) {
        if (texture.value) SDL_DestroyTexture(texture.value);
        if (texture.odd) SDL_DestroyTexture(texture.odd);
        texture.odd = nullptr;
        texture.width = width;
        texture.height = height;
        texture.value = SDL_CreateTexture(dest, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, texture.width, texture.height);
        if (!texture.value) return nullptr;
        SDL_SetTextureBlendMode(texture.value, SDL_BLENDMODE_NONE);
        full = true;
    }
    // A changed filter can retain dimensions (interlace); force both uploads.
    if (texture.filtered != filtered) { texture.filtered = filtered; full = true; }
    if (filtered && texture.crt.fields() && !texture.odd) {
        texture.odd = SDL_CreateTexture(dest, SDL_PIXELFORMAT_XRGB8888,
            SDL_TEXTUREACCESS_STREAMING, texture.width, texture.height);
        if (!texture.odd) return nullptr;
        SDL_SetTextureBlendMode(texture.odd, SDL_BLENDMODE_NONE);
        full = true;
    }
    SDL_SetTextureScaleMode(texture.value, computer->video_system->display_pixel_mode == DM_PIXEL_FUZZ
        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART);
    if (texture.odd) SDL_SetTextureScaleMode(texture.odd, computer->video_system->display_pixel_mode == DM_PIXEL_FUZZ
        ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART);
    if (!video.upload_frame(texture.value, full, &texture.crt, texture.odd)) return nullptr;
    last_renderer = dest;
    // The existing Oak beam includes both fields. Select its current parity,
    // not a wall-clock counter, so pause and replay retain the same picture.
    texture.shown = filtered && texture.crt.fields() && video.vpos() >= timing.total_height / 2
        ? texture.odd : texture.value;
    return texture.shown;
}

// Present the selected monitor(s) without altering card timing. Restore the
// IIgs viewport and draw state after placing the card image.
void SecondSightDisplay::render() {
    // Claim the requested scan without joining its host-frame deadline. A
    // paused machine, the first texture and a missed whole frame still fence.
    // Every return releases ownership, including a hidden/failed monitor.
    struct Release {
        SSCard &card;
        ~Release() { card.end_presentation(); }
    } release{card};
    auto *vs = computer->video_system;
    const int view = SystemSettings::instance().ss_view();
    const uint64_t ticks = computer->clock->get_c14m();
#if defined(__EMSCRIPTEN__)
    const bool missing_texture = !main_texture.value;
#else
    const bool missing_texture = view == SystemSettings::SS_VIEW_BOTH ? !window_texture.value : !main_texture.value;
#endif
    const bool exact = missing_texture || missed_frame || computer->execution_mode != EXEC_NORMAL;
    const bool refresh = view != SystemSettings::SS_VIEW_IIGS && card.begin_presentation(ticks, exact);
    missed_frame = view != SystemSettings::SS_VIEW_IIGS && !refresh;
#if !defined(__EMSCRIPTEN__)
    // A native second window has its own renderer and mouse-capture setting.
    // Use the programmed CRT geometry, shared with the main/browser view;
    // texture dimensions describe samples, not the physical pixel aspect.
    if (view == SystemSettings::SS_VIEW_BOTH && open_window()) {
        if (!visible) { vs->show(window); visible = true; }
        if (mouse_captured != vs->mouse_captured) {
            mouse_captured = vs->mouse_captured;
            SDL_SetWindowRelativeMouseMode(window, mouse_captured);
        }
        SDL_RenderClear(renderer);
        if (auto *texture = prepare_texture(renderer, window_texture, refresh)) {
            int w, h;
            SDL_GetRenderOutputSize(renderer, &w, &h);
            const SDL_FRect dest = frame_rect({0, 0, float(w), float(h)}, vs->aspect_ratio);
            card_content = dest;
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
        }
        SDL_RenderPresent(renderer);
    } else if (visible) {
        vs->hide(window);
        visible = false;
    }
    if (view != SystemSettings::SS_VIEW_CARD) return;
#else
    if (view == SystemSettings::SS_VIEW_IIGS) return;
#endif
    // The browser shares the Apple renderer. Save its viewport and content
    // rectangle so card drawing cannot change later Apple input coordinates.
    const auto target = vs->target;
    const auto content = vs->content;
    if (view == SystemSettings::SS_VIEW_BOTH) vs->target.x += vs->target.w;
    // Only card pixels go in this monitor, even before its firmware sets a mode.
    Uint8 r, g, b, a;
    SDL_BlendMode blend;
    SDL_GetRenderDrawColor(vs->renderer, &r, &g, &b, &a);
    SDL_GetRenderDrawBlendMode(vs->renderer, &blend);
    SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
    SDL_SetRenderDrawBlendMode(vs->renderer, SDL_BLENDMODE_NONE);
    if (auto *texture = prepare_texture(vs->renderer, main_texture, refresh)) {
        SDL_FRect source = {0, 0, float(main_texture.width), float(main_texture.height)};
        // The texture contains active pixels and overscan at one sample scale.
        // Only blank/retrace space outside this complete visible scan is black.
        // update_display already fitted this viewport with the Apple aspect.
        // The opaque complete scan covers it; avoid a redundant fill beneath
        // every browser frame. A missing card signal still clears it below.
        vs->render_frame(texture, &source, nullptr);
        // render_frame receives no destination adjustment or content inset;
        // its retained content is the exact SDL_RenderTexture destination.
        card_content = vs->content;
    } else {
        SDL_RenderFillRect(vs->renderer, &vs->target);
    }
    SDL_SetRenderDrawColor(vs->renderer, r, g, b, a);
    SDL_SetRenderDrawBlendMode(vs->renderer, blend);
    vs->target = target;
    if (view == SystemSettings::SS_VIEW_BOTH) vs->content = content;
}

// Map card-monitor input to the existing IIgs input path; closing the native
// monitor changes the view preference rather than removing the card.
bool SecondSightDisplay::handle_event(const SDL_Event &event) {
    auto *vs = computer->video_system;
#if defined(__EMSCRIPTEN__)
    if (SystemSettings::instance().ss_view() != SystemSettings::SS_VIEW_BOTH || SDL_GetWindowFromEvent(&event) != vs->window)
        return false;
    auto *source_window = vs->window;
    auto *source_renderer = vs->renderer;
#else
    if (!window || SDL_GetWindowFromEvent(&event) != window) return false;
    auto *source_window = window;
    auto *source_renderer = renderer;
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        SystemSettings::instance().set_ss_view(SystemSettings::SS_VIEW_IIGS);
        return true;
    }
#endif
    SDL_Event guest = event;
    const bool motion = event.type == SDL_EVENT_MOUSE_MOTION;
    const bool button = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP;
    if (motion || button) {
        int w, h, pw, ph;
        SDL_GetWindowSize(source_window, &w, &h);
        SDL_GetRenderOutputSize(source_renderer, &pw, &ph);
        if (w < 1 || h < 1 || card_content.w <= 0 || card_content.h <= 0) return false;
        // SDL events use window units; rendering uses physical pixels on high-
        // density screens. Convert to pixels before testing the card rectangle.
        float x = (motion ? event.motion.x : event.button.x) * pw / w;
        float y = (motion ? event.motion.y : event.button.y) * ph / h;
#if defined(__EMSCRIPTEN__)
        const bool inside = x >= card_content.x && x < card_content.x + card_content.w &&
                            y >= card_content.y && y < card_content.y + card_content.h;
        // Once a drag starts in the card image, deliver its release there
        // even if the pointer leaves it. Otherwise the guest button sticks.
        if (!inside && !card_mouse_down) return false;
#endif
        // The IIgs mouse handler maps main-window content coordinates to guest
        // coordinates. Give either monitor the same mapping, without changing it.
        x = vs->content.x + (x - card_content.x) * vs->content.w / card_content.w;
        y = vs->content.y + (y - card_content.y) * vs->content.h / card_content.h;
        SDL_GetWindowSize(vs->window, &w, &h);
        SDL_GetRenderOutputSize(vs->renderer, &pw, &ph);
        if (pw < 1 || ph < 1) return false;
        if (motion) { guest.motion.x = x * w / pw; guest.motion.y = y * h / ph; }
        else {
            guest.button.x = x * w / pw; guest.button.y = y * h / ph;
            card_mouse_down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        computer->dispatch->dispatch(guest);
        return true;
    }
#if defined(__EMSCRIPTEN__)
    return false;
#else
    if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT)
        computer->dispatch->dispatch(guest);
    return true;
#endif
}
