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
#include "util/TextRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <utility>

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
    if (main_texture.lcd_source) SDL_DestroyTexture(main_texture.lcd_source);
    if (window_texture.lcd_source) SDL_DestroyTexture(window_texture.lcd_source);
    lcd_gpu.shutdown(vs->gpu_device);
    // Each text engine belongs to its renderer; release it first.
    window_text.reset();
    main_text.reset();
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
SDL_Texture *SecondSightDisplay::prepare_texture(SDL_Renderer *dest, Texture &texture, bool refresh, SDL_FPoint shown) {
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
    // The web build shows the LCD only with its GPU filter; the CPU filter is
    // too slow for a browser.
    bool on_lcd = video.vga_monitor() && SystemSettings::instance().ss_vga_lcd() && ss_lcd_offered();
#if defined(__EMSCRIPTEN__)
    on_lcd = on_lcd && use_lcd_gpu(dest);
#endif
    const auto signal = on_lcd ? ss_monitor::lcd_signal(video) : ss_monitor::LcdSignal{};
    lcd.follow(signal.present, signal.timing);
    if (on_lcd) {
        const auto shape = ss_monitor::lcd_shape(video, signal);
        if (lcd.needs_picture() || !(shape == lcd_shape)) {
            lcd_shape = shape;
            lcd.picture(ss_monitor::lcd_picture(video, signal));
        }
    }
    const auto scan = on_lcd ? ss_monitor::lcd_scan(video, lcd, signal) : ss_monitor::scan(video);
    texture.aspect = on_lcd ? ss_lcd::aspect : 0;
    const double line_hz = 1.0 / timing.line_period();
    const int fields = timing.interlaced ? 2 : 1;
    // A monitor locks to timing that holds for a whole frame. The states that
    // pass while a mode is programmed never reach the screen. A CRTC with no
    // programmed totals, as after reset, gives no timing to lock to or reject.
    const auto &crtc = video.crtc_state();
    if (crtc.horz_total && crtc.horz_disp_end && crtc.vert_disp_end && scan.locked != lock_candidate) {
        lock_candidate = scan.locked;
        lock_since = video.frame_number();
    }
    const double field_hz = fields / timing.frame_period();
    char message[128];
    if (lock_candidate == out_of_range && video.frame_number() >= lock_since + 2) {
        out_of_range = !lock_candidate;
        if (out_of_range) {
            const char *name = on_lcd ? "VGA LCD" : video.vga_monitor() ? "VGA CRT" : "AppleColor RGB";
            if (on_lcd && !signal.present) {
                snprintf(message, sizeof message, "Second Sight %s: no signal", name);
            } else if (scan.no_sync) {
                snprintf(message, sizeof message, "Second Sight %s: no sync, the card sends no sync pulses", name);
            } else if (scan.bad_sync) {
                snprintf(message, sizeof message, "Second Sight %s: no sync, %s", name,
                    (video.misc_output() & 0x40) ? "VSync must be positive" : "HSync must be negative");
            } else if (on_lcd) {
                snprintf(message, sizeof message, "Second Sight %s: out of range (%.2f kHz, %.2f Hz)", name,
                    line_hz / 1000.0, field_hz);
            } else {
                // Name the rate the monitor cannot follow, with the range it takes.
                const auto &limits = video.vga_monitor() ? ss_monitor::vga_limits : ss_monitor::applecolor_limits;
                const bool line_out = line_hz < limits.line_min || line_hz > limits.line_max;
                const bool field_out = field_hz < limits.field_min || field_hz > limits.field_max;
                char line[48] = "", field[48] = "";
                if (line_out || !field_out)
                    snprintf(line, sizeof line, "lines %.2f kHz (%.2f-%.2f)", line_hz / 1000.0,
                        limits.line_min / 1000.0, limits.line_max / 1000.0);
                if (field_out || !line_out)
                    snprintf(field, sizeof field, "fields %.1f Hz (%.1f-%.1f)", field_hz, limits.field_min,
                        limits.field_max);
                snprintf(message, sizeof message, "Second Sight %s: out of range, %s%s%s", name, line,
                    *line && *field ? ", " : "", field);
            }
            fprintf(stderr, "%s\n", message);
            steady_message = message;
        } else {
            steady_message.clear();
        }
    }
    if (!scan.h_hold && !scan.v_hold && !scan.v_height) {
        hold_line = hold_field = 0;
    } else if (line_hz != hold_line || field_hz != hold_field) {
        hold_line = line_hz;
        hold_field = field_hz;
        hold_since = video.frame_number();
        hold_shown = false;
    }
    if (hold_line && !hold_shown && !out_of_range && video.frame_number() >= hold_since + 2) {
        hold_shown = true;
        std::string controls;
        for (const auto &[turned, name] : {std::pair{scan.h_hold, "H-HOLD"}, {scan.v_hold, "V-HOLD"},
                 {scan.v_height, "V-HEIGHT"}})
            if (turned) controls += (controls.empty() ? "" : ", ") + std::string(name);
        char notice[128];
        snprintf(notice, sizeof notice, "Second Sight AppleColor RGB: this mode needs %s adjusted (%.2f kHz, %.2f Hz)",
            controls.c_str(), line_hz / 1000.0, field_hz);
        fprintf(stderr, "%s\n", notice);
        show_notice(notice);
    }
    const auto &horizontal = scan.horizontal, &vertical = scan.vertical;
    // An LCD has no scan lines and shows the fields of an interlaced signal woven.
    const bool filtered = !on_lcd && computer->video_system->get_crt_shader_enabled();
    const int phase = timing.interlaced ? video.odd_field_sync_phase() : 0;
    const bool odd = timing.interlaced && video.vpos() >= timing.total_height / 2;
    texture.crt.configure(horizontal, vertical, filtered, timing.interlaced, video.border_color().value, !out_of_range,
        phase, int(timing.total_width), odd, timing.interlaced && !on_lcd);
    // On the LCD the texture holds the panel's picture at the size it is
    // shown, or at the panel's own size when it is shown larger, filtered from
    // the monitor's samples: on the GPU when it can, else on the CPU as each
    // changed row arrives.
    bool gpu = false;
    if (on_lcd) {
        if (texture.panel.configure(texture.crt.width(), texture.crt.height(), int(std::lround(shown.x)),
                int(std::lround(shown.y)))) {
            texture.lcd_tables = 0;
            full = true;
        }
        gpu = use_lcd_gpu(dest) && prepare_lcd_source(dest, texture, full);
    }
    if (gpu) {
        texture.crt.sink = [&texture](const uint32_t *pixels, int top, int bottom) {
            const int width = texture.crt.width();
            const SDL_Rect rows = {0, top, width, bottom - top + 1};
            texture.lcd_samples_changed = true;
            return SDL_UpdateTexture(texture.lcd_source, &rows, pixels + size_t(top) * width,
                width * int(sizeof(uint32_t)));
        };
    } else if (on_lcd) {
        texture.crt.sink = [&texture](const uint32_t *pixels, int top, int bottom) {
            const auto [first, last] = texture.panel.update(pixels, top, bottom);
            if (last < first) return true;
            const int width = texture.panel.width();
            const SDL_Rect rows = {0, first, width, last - first + 1};
            return SDL_UpdateTexture(texture.value, &rows, texture.panel.pixels() + size_t(first) * width,
                width * int(sizeof(uint32_t)));
        };
    } else {
        texture.crt.sink = nullptr;
    }
    const int width = on_lcd ? texture.panel.width() : texture.crt.width();
    const int height = on_lcd ? texture.panel.height() : texture.crt.height();
    if (width < 1 || height < 1 || width > OTI087::MAX_FRAME_DIMENSION * 2 || height > OTI087::MAX_FRAME_DIMENSION * 2)
        return nullptr;
    if (!texture.value || texture.width != width || texture.height != height || texture.lcd_gpu != gpu) {
        if (texture.value) SDL_DestroyTexture(texture.value);
        if (texture.odd) SDL_DestroyTexture(texture.odd);
        texture.odd = nullptr;
        texture.width = width;
        texture.height = height;
        texture.lcd_gpu = gpu;
        // The GPU filter draws into the texture, in the byte order it reads.
        texture.value = SDL_CreateTexture(dest, gpu ? SDL_PIXELFORMAT_ABGR8888 : SDL_PIXELFORMAT_XRGB8888,
            gpu ? SDL_TEXTUREACCESS_TARGET : SDL_TEXTUREACCESS_STREAMING, texture.width, texture.height);
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
    if (gpu && texture.lcd_samples_changed) {
        texture.lcd_samples_changed = false;
        if (!lcd_gpu.run(texture.lcd_source, texture.value, {texture.crt.height(), width, height})) return nullptr;
    }
    last_renderer = dest;
    // The existing Oak beam includes both fields. Select its current parity,
    // not a wall-clock counter, so pause and replay retain the same picture.
    texture.shown = filtered && texture.crt.fields() && video.vpos() >= timing.total_height / 2
        ? texture.odd : texture.value;
    return texture.shown;
}

// The GPU filter works on the main window's renderer. Set it up on first use.
bool SecondSightDisplay::use_lcd_gpu(SDL_Renderer *dest) {
    auto *vs = computer->video_system;
    if (dest != vs->renderer) return false;
    if (!lcd_gpu_tried) {
        lcd_gpu_tried = true;
        lcd_gpu.init(vs->renderer, vs->gpu_device);
    }
    return lcd_gpu.ready();
}

// Size the GPU filter's input for this capture and these taps: the samples in
// the top rows, the tap tables below. Writes the tables when they changed and
// sets full when every sample row must be sent again. False when this size
// needs more taps than the shader reads, so the CPU filters it.
bool SecondSightDisplay::prepare_lcd_source(SDL_Renderer *dest, Texture &texture, bool &full) {
    if (texture.lcd_tables < 0) return false;
    constexpr int taps = ss_lcd::Panel::gpu_taps;
    const int width = std::max({texture.crt.width(), texture.panel.width(), texture.panel.height()});
    const int height = texture.crt.height() + 2 * taps;
    if (!texture.lcd_source || texture.lcd_source_width != width || texture.lcd_source_height != height) {
        if (texture.lcd_source) SDL_DestroyTexture(texture.lcd_source);
        texture.lcd_source = SDL_CreateTexture(dest, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
            width, height);
        if (!texture.lcd_source) return false;
        SDL_SetTextureBlendMode(texture.lcd_source, SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(texture.lcd_source, SDL_SCALEMODE_NEAREST);
        texture.lcd_source_width = width;
        texture.lcd_source_height = height;
        texture.lcd_tables = 0;
        full = true;
    }
    if (texture.lcd_tables == 0) {
        std::vector<uint32_t> rows(size_t(2 * taps) * width);
        if (!texture.panel.gpu_tables(rows.data(), width)) {
            texture.lcd_tables = -1;
            return false;
        }
        const SDL_Rect area = {0, texture.crt.height(), width, 2 * taps};
        if (!SDL_UpdateTexture(texture.lcd_source, &area, rows.data(), width * int(sizeof(uint32_t)))) return false;
        texture.lcd_tables = 1;
    }
    return true;
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
        int w, h;
        SDL_GetRenderOutputSize(renderer, &w, &h);
        const SDL_FRect area = frame_rect({0, 0, float(w), float(h)}, ss_lcd::aspect);
        if (auto *texture = prepare_texture(renderer, window_texture, refresh, {area.w, area.h})) {
            const SDL_FRect dest = frame_rect({0, 0, float(w), float(h)},
                window_texture.aspect > 0 ? window_texture.aspect : vs->aspect_ratio);
            card_content = dest;
            SDL_RenderTexture(renderer, texture, nullptr, &dest);
        }
        draw_screen_messages(renderer, window_text, {0, 0, float(w), float(h)});
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
    const SDL_FRect area = frame_rect(vs->target, ss_lcd::aspect);
    if (auto *texture = prepare_texture(vs->renderer, main_texture, refresh, {area.w, area.h})) {
        SDL_FRect source = {0, 0, float(main_texture.width), float(main_texture.height)};
        // The texture contains active pixels and overscan at one sample scale.
        // Only blank/retrace space outside this complete visible scan is black.
        // update_display already fitted this viewport with the Apple aspect.
        // The opaque complete scan covers it; avoid a redundant fill beneath
        // every browser frame. A missing card signal still clears it below.
        // An LCD panel shows its picture at its own aspect, with black edges.
        if (main_texture.aspect > 0) {
            SDL_RenderFillRect(vs->renderer, &vs->target);
            const SDL_FRect panel = frame_rect(vs->target, main_texture.aspect);
            SDL_FRect inset = {0, 0, (vs->target.w - panel.w) * 0.5f * source.w / vs->target.w,
                               (vs->target.h - panel.h) * 0.5f * source.h / vs->target.h};
            vs->render_frame(texture, &source, &inset);
        } else {
            vs->render_frame(texture, &source, nullptr);
        }
        // render_frame receives no destination adjustment or content inset;
        // its retained content is the exact SDL_RenderTexture destination.
        card_content = vs->content;
    } else {
        SDL_RenderFillRect(vs->renderer, &vs->target);
    }
    draw_screen_messages(vs->renderer, main_text, vs->target);
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

// A notice lasts 4 s, over the picture or over a steady message.
void SecondSightDisplay::show_notice(const std::string &text) {
    notice = text;
    notice_until = SDL_GetTicksNS() + 4'000'000'000ull;
}

// The steady message is centered: the monitor has no picture then. A notice
// sits near the top, over a picture. The font follows the monitor height in
// output pixels.
void SecondSightDisplay::draw_screen_messages(SDL_Renderer *dest, std::unique_ptr<TextRenderer> &text,
                                              const SDL_FRect &area) {
    if (!notice.empty() && SDL_GetTicksNS() >= notice_until) notice.clear();
    if ((steady_message.empty() && notice.empty()) || area.w < 1 || area.h < 1) return;
    const float size = std::clamp(std::round(area.h / 22.0f), 12.0f, 44.0f);
    if (!text) {
        text = std::make_unique<TextRenderer>(dest, "fonts/OpenSans-Regular.ttf", size);
        text->font_size = size;
    } else if (text->font && text->font_size != size && TTF_SetFontSize(text->font, size)) {
        text->font_size = size;
        text->font_line_height = TTF_GetFontHeight(text->font);
    }
    Uint8 r, g, b, a;
    SDL_BlendMode blend;
    SDL_GetRenderDrawColor(dest, &r, &g, &b, &a);
    SDL_GetRenderDrawBlendMode(dest, &blend);
    SDL_SetRenderDrawBlendMode(dest, SDL_BLENDMODE_BLEND);
    if (!steady_message.empty()) draw_message_box(*text, dest, steady_message, area, true);
    if (!notice.empty()) draw_message_box(*text, dest, notice, area, false);
    SDL_SetRenderDrawColor(dest, r, g, b, a);
    SDL_SetRenderDrawBlendMode(dest, blend);
}

// One message in a dark box. A message wider than 90% of the monitor breaks
// after its first colon (monitor name, then the problem), then at spaces.
void SecondSightDisplay::draw_message_box(TextRenderer &text, SDL_Renderer *dest, const std::string &message,
                                          const SDL_FRect &area, bool centered) {
    const float limit = area.w * 0.9f;
    std::vector<std::string> parts{message}, lines;
    const size_t colon = message.find(": ");
    if (colon != std::string::npos && text.string_width(message) > limit)
        parts = {message.substr(0, colon + 1), message.substr(colon + 2)};
    for (const auto &part : parts) {
        std::string line;
        size_t start = 0;
        while (start < part.size()) {
            size_t end = part.find(' ', start);
            if (end == std::string::npos) end = part.size();
            const std::string word = part.substr(start, end - start);
            const std::string wider = line.empty() ? word : line + " " + word;
            if (!line.empty() && text.string_width(wider) > limit) {
                lines.push_back(line);
                line = word;
            } else {
                line = wider;
            }
            start = end + 1;
        }
        if (!line.empty()) lines.push_back(line);
    }
    float width = 0;
    for (const auto &l : lines) width = std::max(width, float(text.string_width(l)));
    const float pad = std::round(text.font_size * 0.6f), line_height = float(text.get_font_line_height());
    SDL_FRect box = {std::round(area.x + (area.w - width) * 0.5f - pad), 0, width + 2 * pad,
                     lines.size() * line_height + 2 * pad};
    box.y = std::round(centered ? area.y + (area.h - box.h) * 0.5f : area.y + area.h * 0.06f);
    SDL_SetRenderDrawColor(dest, 0, 0, 0, 200);
    SDL_RenderFillRect(dest, &box);
    text.set_color(0xff, 0xff, 0xff, 0xff);
    for (size_t i = 0; i < lines.size(); ++i)
        text.render(lines[i], int(area.x + area.w * 0.5f), int(box.y + pad + i * line_height), TEXT_ALIGN_CENTER);
}
