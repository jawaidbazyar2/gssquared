#include "debugger/DebugVideoView.hpp"

#include <algorithm>
#include <cctype>
#include <new>

#include "devices/displaypp/CharRom.hpp"
#include "mmus/mmu.hpp"
#include "mmus/mmu_iigs.hpp"

const char *DebugVideoView::decode_name(video_decode_mode_t m) {
    switch (m) {
        case video_decode_mode_t::TEXT40: return "TEXT40";
        case video_decode_mode_t::TEXT80: return "TEXT80";
        case video_decode_mode_t::LORES40: return "LORES40";
        case video_decode_mode_t::LORES80: return "LORES80";
        case video_decode_mode_t::HIRES: return "HIRES";
        case video_decode_mode_t::HIRES_NOSHIFT: return "HIRES_NS";
        case video_decode_mode_t::DHGR: return "DHGR";
        case video_decode_mode_t::SHR: return "SHR";
    }
    return "?";
}

const char *DebugVideoView::render_name(video_render_mode_t m) {
    switch (m) {
        case video_render_mode_t::MONO: return "MONO";
        case video_render_mode_t::NTSC: return "NTSC";
        case video_render_mode_t::RGB: return "RGB";
    }
    return "?";
}

video_decode_mode_t DebugVideoView::next_decode(video_decode_mode_t m) {
    int v = static_cast<int>(m) + 1;
    if (v > static_cast<int>(video_decode_mode_t::SHR)) {
        v = 0;
    }
    return static_cast<video_decode_mode_t>(v);
}

video_render_mode_t DebugVideoView::next_render(video_render_mode_t m) {
    int v = static_cast<int>(m) + 1;
    if (v > static_cast<int>(video_render_mode_t::RGB)) {
        v = 0;
    }
    return static_cast<video_render_mode_t>(v);
}

bool DebugVideoView::mode_needs_aux(video_decode_mode_t m) {
    return m == video_decode_mode_t::TEXT80 || m == video_decode_mode_t::LORES80 ||
           m == video_decode_mode_t::DHGR;
}

size_t DebugVideoView::mode_main_size(video_decode_mode_t m) {
    switch (m) {
        case video_decode_mode_t::TEXT40:
        case video_decode_mode_t::TEXT80:
        case video_decode_mode_t::LORES40:
        case video_decode_mode_t::LORES80:
            return 0x400;
        case video_decode_mode_t::HIRES:
        case video_decode_mode_t::HIRES_NOSHIFT:
        case video_decode_mode_t::DHGR:
            return 0x2000;
        case video_decode_mode_t::SHR:
            return 0x8000;
    }
    return 0;
}

size_t DebugVideoView::mode_aux_size(video_decode_mode_t m) {
    if (!mode_needs_aux(m)) {
        return 0;
    }
    return mode_main_size(m);
}

uint32_t DebugVideoView::default_aux_address(uint32_t main_addr) {
    uint8_t bank = (main_addr >> 16) & 0xFF;
    uint16_t off = main_addr & 0xFFFF;
    return (static_cast<uint32_t>(bank ^ 0x01) << 16) | off;
}

bool DebugVideoView::apply_preset(const std::string &name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (n == "text1") {
        decode = video_decode_mode_t::TEXT40;
        address = 0x0400;
        return true;
    }
    if (n == "text2") {
        decode = video_decode_mode_t::TEXT40;
        address = 0x0800;
        return true;
    }
    if (n == "80text1") {
        decode = video_decode_mode_t::TEXT80;
        address = 0x0400;
        return true;
    }
    if (n == "80text2") {
        decode = video_decode_mode_t::TEXT80;
        address = 0x0800;
        return true;
    }
    if (n == "gr1") {
        decode = video_decode_mode_t::LORES40;
        address = 0x0400;
        return true;
    }
    if (n == "gr2") {
        decode = video_decode_mode_t::LORES40;
        address = 0x0800;
        return true;
    }
    if (n == "hgr1") {
        decode = video_decode_mode_t::HIRES;
        address = 0x2000;
        return true;
    }
    if (n == "hgr2") {
        decode = video_decode_mode_t::HIRES;
        address = 0x4000;
        return true;
    }
    if (n == "dhgr1") {
        decode = video_decode_mode_t::DHGR;
        address = 0x2000;
        return true;
    }
    if (n == "dhgr2") {
        decode = video_decode_mode_t::DHGR;
        address = 0x4000;
        return true;
    }
    if (n == "shr") {
        decode = video_decode_mode_t::SHR;
        address = 0xE12000;
        return true;
    }
    return false;
}

const uint8_t *DebugVideoView::guest_ptr(MMU *mmu, uint32_t start, size_t len, bool *shr_interleave) {
    if (shr_interleave) {
        *shr_interleave = false;
    }
    if (!mmu || len == 0) {
        return nullptr;
    }

    auto *gs = dynamic_cast<MMU_IIgs *>(mmu);
    uint8_t bank = (start >> 16) & 0xFF;
    uint16_t off = start & 0xFFFF;

    // IIgs Mega II: E0 is linear main, E1 is aux. With C029 linear/SHR the
    // $2000–$9FFF aux window is physically interleaved; the renderer maps
    // CPU-linear offsets onto that layout (see iigs_aux_linear_to_phys).
    if (gs && gs->megaii) {
        uint8_t *ram = gs->megaii->get_memory_base();
        uint32_t msz = gs->megaii->get_memory_size();
        if (ram && bank == 0xE1) {
            uint32_t phys = 0x10000u + off;
            if (phys <= msz && len <= msz - phys) {
                if (shr_interleave) {
                    *shr_interleave = gs->is_aux_linear();
                }
                return ram + phys;
            }
            return nullptr;
        }
        if (ram && bank == 0xE0) {
            if (static_cast<uint32_t>(off) <= 0x10000u && len <= 0x10000u - off) {
                return ram + off;
            }
            return nullptr;
        }
    }

    uint8_t *base = mmu->get_memory_base();
    uint32_t mem_size = mmu->get_memory_size();
    if (base && bank == 0 && start <= mem_size && len <= mem_size - start) {
        return base + start;
    }
    if (base && bank == 0x01) {
        uint32_t flat = 0x10000u + off;
        if (flat <= mem_size && len <= mem_size - flat) {
            return base + flat;
        }
    }
    return nullptr;
}

void DebugVideoView::ensure_frames(SDL_Renderer *renderer) {
    if (bound_renderer_ == renderer && frame_rgba_ && frame_shr_) {
        return;
    }
    bound_renderer_ = renderer;
    frame_rgba_.reset();
    frame_shr_.reset();

    frame_rgba_ = std::unique_ptr<Frame560RGBA>(
        new (std::align_val_t(64)) Frame560RGBA(567, 192, renderer, PIXEL_FORMAT));
    frame_shr_ = std::unique_ptr<Frame640>(
        new (std::align_val_t(64)) Frame640(640, 200, renderer, PIXEL_FORMAT));

    if (SDL_Texture *t = frame_rgba_->get_texture()) {
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }
    if (SDL_Texture *t = frame_shr_->get_texture()) {
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }
}

void DebugVideoView::sync_generator(const DebugVideoDecodeOpts &opts) {
    CharRom *rom = opts.char_rom;
    if (!rom) {
        generator_.reset();
        bound_rom_ = nullptr;
        return;
    }
    if (!generator_ || bound_rom_ != rom) {
        bound_rom_ = rom;
        generator_ = std::make_unique<AppleII_View>(rom);
    }
    generator_->set_flash_state(opts.flash_state);
    generator_->set_normal_alt(opts.altcharset);
    generator_->set_char_set(opts.char_set);
    generator_->set_text_fg(opts.text_fg);
    generator_->set_text_bg(opts.text_bg);
}

void DebugVideoView::rebuild(MMU *mmu, SDL_Renderer *renderer, const DebugVideoDecodeOpts &opts) {
    if (!renderer || !mmu) {
        return;
    }
    ensure_frames(renderer);
    sync_generator(opts);
    if (!generator_) {
        return;
    }

    size_t main_sz = mode_main_size(decode);
    size_t aux_sz = mode_aux_size(decode);
    bool shr_interleave = false;
    const uint8_t *main = guest_ptr(mmu, address, main_sz, &shr_interleave);
    const uint8_t *aux = nullptr;
    if (aux_sz > 0) {
        aux = guest_ptr(mmu, default_aux_address(address), aux_sz, nullptr);
    }

    if (decode == video_decode_mode_t::SHR) {
        frame_shr_->open();
        generator_->generate(decode, render, main, nullptr, nullptr, frame_shr_.get(), shr_interleave);
        frame_shr_->close();
        return;
    }

    frame_rgba_->open();
    generator_->generate(decode, render, main, aux, frame_rgba_.get(), nullptr);
    frame_rgba_->close();
}

SDL_Texture *DebugVideoView::texture() const {
    if (decode == video_decode_mode_t::SHR) {
        return frame_shr_ ? frame_shr_->get_texture() : nullptr;
    }
    return frame_rgba_ ? frame_rgba_->get_texture() : nullptr;
}

int DebugVideoView::width() const {
    return decode == video_decode_mode_t::SHR ? 640 : 560;
}

int DebugVideoView::height() const {
    return decode == video_decode_mode_t::SHR ? 200 : 192;
}

uint32_t DebugVideoViews::add(video_decode_mode_t decode, uint32_t address, video_render_mode_t render) {
    DebugVideoView v;
    v.id = next_id_++;
    v.decode = decode;
    v.address = address;
    v.render = render;
    views_.push_back(std::move(v));
    return views_.back().id;
}

uint32_t DebugVideoViews::add_preset(const std::string &name) {
    DebugVideoView v;
    if (!v.apply_preset(name)) {
        return 0;
    }
    v.id = next_id_++;
    views_.push_back(std::move(v));
    return views_.back().id;
}

bool DebugVideoViews::remove(uint32_t id) {
    auto it = std::find_if(views_.begin(), views_.end(),
                           [id](const DebugVideoView &v) { return v.id == id; });
    if (it == views_.end()) {
        return false;
    }
    views_.erase(it);
    return true;
}

void DebugVideoViews::clear() {
    views_.clear();
}

DebugVideoView *DebugVideoViews::find(uint32_t id) {
    for (auto &v : views_) {
        if (v.id == id) {
            return &v;
        }
    }
    return nullptr;
}
