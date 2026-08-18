#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "devices/displaypp/generate/AppleII.hpp"

struct computer_t;
class MMU;
class CharRom;

/** Options synced from the live display each frame (or left default). */
struct DebugVideoDecodeOpts {
    CharRom *char_rom = nullptr;
    bool flash_state = false;
    bool altcharset = false;
    uint16_t char_set = 0;
    uint8_t text_fg = 0x0F;
    uint8_t text_bg = 0x00;
};

/** One debug thumbnail: decode mode, render mode, base address. */
struct DebugVideoView {
    uint32_t id = 0;
    video_decode_mode_t decode = video_decode_mode_t::HIRES;
    video_render_mode_t render = video_render_mode_t::RGB;
    uint32_t address = 0x2000; // 24-bit BBAAAA

    /** Rebuild textures from current MMU memory. Call each debug frame. */
    void rebuild(MMU *mmu, SDL_Renderer *renderer, const DebugVideoDecodeOpts &opts);

    SDL_Texture *texture() const;
    int width() const;
    int height() const;
    /** On-screen blit size: 1× horizontal, 2× vertical (Apple II pixel aspect). */
    int display_width() const { return width(); }
    int display_height() const { return height() * 2; }

    static const char *decode_name(video_decode_mode_t m);
    static const char *render_name(video_render_mode_t m);
    static video_decode_mode_t next_decode(video_decode_mode_t m);
    static video_render_mode_t next_render(video_render_mode_t m);
    static bool mode_needs_aux(video_decode_mode_t m);
    static size_t mode_main_size(video_decode_mode_t m);
    static size_t mode_aux_size(video_decode_mode_t m);

    /** Apply a named preset (text1, hgr1, …). Returns false if unknown. */
    bool apply_preset(const std::string &name);

private:
    std::unique_ptr<Frame560RGBA> frame_rgba_;
    std::unique_ptr<Frame640> frame_shr_;
    CharRom *bound_rom_ = nullptr;
    std::unique_ptr<AppleII_View> generator_;
    SDL_Renderer *bound_renderer_ = nullptr;

    void ensure_frames(SDL_Renderer *renderer);
    void sync_generator(const DebugVideoDecodeOpts &opts);
    static const uint8_t *guest_ptr(MMU *mmu, uint32_t start, size_t len, bool *shr_interleave);
    static uint32_t default_aux_address(uint32_t main_addr);
};

class DebugVideoViews {
    std::vector<DebugVideoView> views_;
    uint32_t next_id_ = 1;

public:
    uint32_t add(video_decode_mode_t decode, uint32_t address, video_render_mode_t render = video_render_mode_t::RGB);
    /** Add from preset name; returns id or 0. */
    uint32_t add_preset(const std::string &name);
    bool remove(uint32_t id);
    void clear();
    int size() const { return static_cast<int>(views_.size()); }

    using iterator = std::vector<DebugVideoView>::iterator;
    using const_iterator = std::vector<DebugVideoView>::const_iterator;
    iterator begin() { return views_.begin(); }
    iterator end() { return views_.end(); }
    const_iterator begin() const { return views_.begin(); }
    const_iterator end() const { return views_.end(); }

    DebugVideoView *find(uint32_t id);
};
