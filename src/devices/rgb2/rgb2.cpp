/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "rgb2.hpp"

#include "Device_ID.hpp"
#include "NClock.hpp"
#include "SlotData.hpp"
#include "cpu.hpp"
#include "mmus/mmu_iigs.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "videosystem.hpp"

#include <cstring>
#include <memory>

namespace {

constexpr uint32_t kStoreBytes = 0x100000;
constexpr uint32_t kPixelLimit = 0xF0000; // $D00000–$DEFFFF
constexpr uint32_t kScbBase = 0xF0000;    // $DF0000
constexpr uint32_t kPaletteBase = 0xF0200; // $DF0200
constexpr int kTexW = 1024;
constexpr int kTexH = 768;
constexpr int kRegCount = 13;

struct sdl_texture_delete {
    void operator()(SDL_Texture *t) const {
        if (t) {
            SDL_DestroyTexture(t);
        }
    }
};

struct rgb2_data : public SlotData {
    computer_t *computer = nullptr;
    MMU_IIgs *mmu = nullptr;
    std::unique_ptr<uint8_t[]> store;
    std::unique_ptr<SDL_Texture, sdl_texture_delete> tex;
    uint16_t reg[kRegCount] = {};
    uint16_t index = 0;
    uint16_t value_latch = 0;
    uint16_t adopted_depth = 0;
    uint16_t adopted_width = 0;
    uint16_t adopted_height = 0;
    uint16_t adopted_page = 0;
    bool adopt_armed = false;

    void reset_regs() {
        for (int i = 0; i < kRegCount; i++) {
            reg[i] = 0;
        }
        index = 0;
        value_latch = 0;
        adopted_depth = 0;
        adopted_width = 0;
        adopted_height = 0;
        adopted_page = 0;
        adopt_armed = false;
    }
};

void write_half(uint16_t &word, bool high, uint8_t value) {
    if (high) {
        word = static_cast<uint16_t>((word & 0x00FF) | (static_cast<uint16_t>(value) << 8));
    } else {
        word = static_cast<uint16_t>((word & 0xFF00) | value);
    }
}

void commit_value(rgb2_data *d) {
    if (d->index >= 12) {
        return;
    }
    uint16_t v = d->value_latch;
    if (d->index == 3) {
        v = static_cast<uint16_t>(v & 0x0FFF);
    }
    d->reg[d->index] = v;
    if (d->index == 11) {
        if (v == 1) {
            d->adopt_armed = false;
        } else if (v == 0) {
            d->adopt_armed = true;
        }
    }
}

uint16_t selected_value(const rgb2_data *d) {
    if (d->index >= kRegCount) {
        return 0;
    }
    return d->reg[d->index];
}

uint8_t rgb2_read_c0xx(void *context, uint32_t address) {
    auto *d = static_cast<rgb2_data *>(context);
    const uint16_t value = selected_value(d);
    switch (address & 0xFF) {
        case 0x69: return static_cast<uint8_t>(d->index & 0xFF);
        case 0x6A: return static_cast<uint8_t>((d->index >> 8) & 0xFF);
        case 0x6B: return static_cast<uint8_t>(value & 0xFF);
        case 0x6C: return static_cast<uint8_t>((value >> 8) & 0xFF);
        default: return 0;
    }
}

void rgb2_write_c0xx(void *context, uint32_t address, uint8_t value) {
    auto *d = static_cast<rgb2_data *>(context);
    switch (address & 0xFF) {
        case 0x69:
            write_half(d->index, false, value);
            return;
        case 0x6A:
            write_half(d->index, true, value);
            return;
        case 0x6B:
            write_half(d->value_latch, false, value);
            commit_value(d);
            return;
        case 0x6C:
            write_half(d->value_latch, true, value);
            commit_value(d);
            return;
        default:
            return;
    }
}

bool enhanced_depth(uint16_t depth) {
    return depth == 2 || depth == 4 || depth == 8 || depth == 16;
}

int stride_for(uint16_t depth, int width) {
    if (width <= 0) {
        return -1;
    }
    switch (depth) {
        case 2: return (width % 4) == 0 ? width / 4 : -1;
        case 4: return (width % 2) == 0 ? width / 2 : -1;
        case 8: return width;
        case 16: return width * 2;
        default: return -1;
    }
}

uint16_t load16(const uint8_t *store, uint32_t offset) {
    if (offset + 1 >= kStoreBytes) {
        return 0;
    }
    return static_cast<uint16_t>(store[offset] | (static_cast<uint16_t>(store[offset + 1]) << 8));
}

uint16_t pixel_color(const uint8_t *store, uint16_t depth, int stride, uint32_t origin, int x, int y) {
    const uint32_t line_off = origin + static_cast<uint32_t>(y) * static_cast<uint32_t>(stride);
    if (depth == 16) {
        const uint32_t off = line_off + static_cast<uint32_t>(x) * 2;
        if (off >= kPixelLimit) {
            return 0;
        }
        return static_cast<uint16_t>(load16(store, off) & 0x7FFF);
    }
    if (depth == 8) {
        const uint32_t off = line_off + static_cast<uint32_t>(x);
        if (off >= kPixelLimit) {
            return 0;
        }
        const uint32_t idx = store[off];
        return static_cast<uint16_t>(load16(store, kPaletteBase + idx * 2) & 0x7FFF);
    }

    const uint8_t scb = (y >= 0 && y < 512) ? store[kScbBase + static_cast<uint32_t>(y)] : 0;
    const uint32_t pal = kPaletteBase + static_cast<uint32_t>(scb & 0x0F) * 32;
    uint8_t entry = 0;
    if (depth == 4) {
        const uint32_t off = line_off + static_cast<uint32_t>(x / 2);
        if (off >= kPixelLimit) {
            return 0;
        }
        const uint8_t b = store[off];
        entry = (x & 1) ? static_cast<uint8_t>(b & 0x0F) : static_cast<uint8_t>(b >> 4);
    } else {
        const uint32_t off = line_off + static_cast<uint32_t>(x / 4);
        if (off >= kPixelLimit) {
            return 0;
        }
        const uint8_t b = store[off];
        const int shift = 6 - 2 * (x & 3);
        const uint8_t p = static_cast<uint8_t>((b >> shift) & 0x3);
        entry = static_cast<uint8_t>(((x & 3) << 2) | p);
    }
    return static_cast<uint16_t>(load16(store, pal + static_cast<uint32_t>(entry) * 2) & 0x7FFF);
}

bool lock_and_fill(rgb2_data *d, int width, int height, bool black) {
    if (!d->tex || width <= 0 || height <= 0) {
        return false;
    }
    void *pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(d->tex.get(), nullptr, &pixels, &pitch)) {
        return false;
    }
    auto *base = static_cast<uint8_t *>(pixels);
    const int stride = stride_for(d->adopted_depth, d->adopted_width);
    const uint32_t origin = static_cast<uint32_t>(d->adopted_page) << 8;
    const uint8_t *store = d->store.get();
    for (int y = 0; y < height; y++) {
        auto *row = reinterpret_cast<uint16_t *>(base + static_cast<size_t>(y) * static_cast<size_t>(pitch));
        if (black || stride < 0 || !store) {
            std::memset(row, 0, static_cast<size_t>(width) * sizeof(uint16_t));
            continue;
        }
        for (int x = 0; x < width; x++) {
            row[x] = pixel_color(store, d->adopted_depth, stride, origin, x, y);
        }
    }
    SDL_UnlockTexture(d->tex.get());

    SDL_FRect src = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)};
    d->computer->video_system->render_frame_vga(d->tex.get(), &src, 0, 0, 0);
    return true;
}

int clamped(uint16_t value, int limit) {
    if (value > limit) {
        return limit;
    }
    return value;
}

bool rgb2_frame(rgb2_data *d, bool /*force_full_frame*/) {
    if (!d->computer || !d->computer->video_system) {
        return false;
    }
    // Register 11 blanks every scanner, classic included, and drops a pending adoption.
    if (d->reg[11] == 1) {
        d->adopt_armed = false;
        const int w = clamped(d->adopted_width, kTexW);
        const int h = clamped(d->adopted_height, kTexH);
        if (w > 0 && h > 0) {
            lock_and_fill(d, w, h, true);
        }
        return true;
    }
    if (d->adopt_armed) {
        d->adopted_depth = d->reg[0];
        d->adopted_width = d->reg[1];
        d->adopted_height = d->reg[2];
        d->adopted_page = static_cast<uint16_t>(d->reg[3] & 0x0FFF);
        d->adopt_armed = false;
    }
    // $C029 bit 7 clear shows the Apple II modes whatever the depth.
    if (d->mmu && !d->mmu->is_shr_enabled()) {
        return false;
    }
    if (!enhanced_depth(d->adopted_depth)) {
        return false;
    }
    const int w = clamped(d->adopted_width, kTexW);
    const int h = clamped(d->adopted_height, kTexH);
    if (w <= 0 || h <= 0) {
        return false;
    }
    if (stride_for(d->adopted_depth, d->adopted_width) < 0) {
        lock_and_fill(d, w, h, true);
        return true;
    }
    return lock_and_fill(d, w, h, false);
}

} // namespace

void init_rgb2(computer_t *computer, SlotType_t slot) {
    auto *d = new rgb2_data();
    d->computer = computer;
    d->id = DEVICE_ID_RGB2;
    d->_slot = slot;
    d->store = std::make_unique<uint8_t[]>(kStoreBytes);

    if (computer->cpu && computer->cpu->mmu) {
        if (auto *gs = dynamic_cast<MMU_IIgs *>(computer->cpu->mmu)) {
            d->mmu = gs;
            for (int bank = 0xD0; bank <= 0xDF; bank++) {
                uint8_t *page = d->store.get() + static_cast<uint32_t>(bank - 0xD0) * 0x10000;
                gs->map_page_both(static_cast<page_t>(bank), page, "RGB2");
                gs->set_page_shadow(static_cast<page_t>(bank), {nullptr, nullptr});
            }
        }
    }

    if (computer->video_system && computer->video_system->renderer) {
        SDL_Texture *tex = SDL_CreateTexture(
            computer->video_system->renderer,
            SDL_PIXELFORMAT_XRGB1555,
            SDL_TEXTUREACCESS_STREAMING,
            kTexW,
            kTexH);
        if (tex) {
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
            d->tex.reset(tex);
        }
    }

    for (uint16_t addr = 0xC069; addr <= 0xC06C; addr++) {
        computer->mmu->set_C0XX_read_handler(addr, {rgb2_read_c0xx, d});
        computer->mmu->set_C0XX_read_replaces(addr);
        computer->mmu->set_C0XX_write_handler(addr, {rgb2_write_c0xx, d});
    }

    if (computer->video_system) {
        computer->video_system->register_frame_processor(1, [d, computer](bool force_full_frame) -> bool {
            bool ret = rgb2_frame(d, force_full_frame);
            if (ret && computer->clock && computer->clock->get_video_scanner()) {
                computer->clock->get_video_scanner()->get_frame_scan()->clear();
            }
            return ret;
        });
    }

    computer->register_reset_handler([d](bool /*cold_start*/) {
        d->reset_regs();
        return true;
    });

    computer->register_shutdown_handler([d]() {
        delete d;
        return true;
    });

    computer->register_debug_display_handler("rgb2", DH_RGB2, [d]() -> DebugFormatter * {
        DebugFormatter *df = new DebugFormatter();
        df->addLine("RGB2.0 slot %d", static_cast<int>(d->_slot));
        df->addLine("  index %04X  latch %04X  adopt_armed %d", d->index, d->value_latch, d->adopt_armed ? 1 : 0);
        df->addLine("  depth %u  width %u  height %u  page %03X", d->reg[0], d->reg[1], d->reg[2], d->reg[3]);
        df->addLine("  scanner %u  display %u  capabilities %u", d->reg[4], d->reg[11], d->reg[12]);
        df->addLine("  adopted depth %u  %ux%u  page %03X",
                    d->adopted_depth, d->adopted_width, d->adopted_height, d->adopted_page);
        return df;
    });
}
