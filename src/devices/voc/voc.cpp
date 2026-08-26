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

#include "voc.hpp"

#include "Device_ID.hpp"
#include "SlotData.hpp"
#include "NClock.hpp"
#include "cpu.hpp"
#include "display/display.hpp"
#include "mmus/mmu_iigs.hpp"
#include "devices/displaypp/RGBA.hpp"
#include "devices/displaypp/generate/AppleII.hpp"
#include "devices/displaypp/frame/Frames.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "videosystem.hpp"

#include <memory>

namespace {

constexpr uint8_t VOC_REG_COUNT = 16;
constexpr uint8_t VOC_RESET_C0B1 = 0x0D;
constexpr uint8_t VOC_RESET_C0B3 = 0x07;
constexpr uint8_t VOC_RESET_C0B4 = 0x00;
constexpr uint8_t VOC_RESET_C0B5 = 0x40;
constexpr uint8_t VOC_RESET_C0B6 = 0x00;

constexpr uint32_t MEGAII_E0_SHR = 0x2000;
constexpr uint32_t MEGAII_E1_SHR = 0x12000;
constexpr uint32_t SHR_WINDOW = 0x8000;

struct voc_data : public SlotData {
    computer_t *computer = nullptr;
    uint8_t reg[VOC_REG_COUNT] = {};
    bool field_parity = false;
    std::unique_ptr<AppleII_View> view;
    std::unique_ptr<Frame640x400> frame;

    voc_data() {
        id = DEVICE_ID_VOC;
        reset_regs();
    }

    void reset_regs() {
        for (uint8_t i = 0; i < VOC_REG_COUNT; i++) {
            reg[i] = 0;
        }
        reg[1] = VOC_RESET_C0B1;
        reg[3] = VOC_RESET_C0B3;
        reg[4] = VOC_RESET_C0B4;
        reg[5] = VOC_RESET_C0B5;
        reg[6] = VOC_RESET_C0B6;
        field_parity = false;
    }

    bool interlace_enabled() const {
        return (reg[1] & 0x30) == 0x30 && (reg[5] & 0x80) != 0;
    }
};

bool host_shr_enabled(computer_t *computer) {
    auto *ds = static_cast<display_state_t *>(computer->get_module_state(MODULE_DISPLAY));
    return ds && (ds->new_video & 0x80) != 0;
}

bool voc_in_vbl(computer_t *computer) {
    if (!computer->clock) {
        return false;
    }
    VideoScannerII *scanner = computer->clock->get_video_scanner();
    return scanner && scanner->is_vbl();
}

uint8_t voc_read_c0xx(void *context, uint32_t address) {
    auto *d = static_cast<voc_data *>(context);
    const uint8_t off = static_cast<uint8_t>(address & 0x0F);
    switch (off) {
        case 0x00: {
            uint8_t status = 0;
            if (voc_in_vbl(d->computer)) {
                status |= (1u << 2);
            }
            if (d->field_parity) {
                status |= (1u << 5);
            }
            return status;
        }
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            return d->reg[off];
        default:
            return 0x00;
    }
}

void voc_write_c0xx(void *context, uint32_t address, uint8_t value) {
    auto *d = static_cast<voc_data *>(context);
    const uint8_t off = static_cast<uint8_t>(address & 0x0F);
    switch (off) {
        case 0x00:
            // Write $00 clears VBL interrupt pending (stub in Phase 1).
            return;
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
            d->reg[off] = value;
            return;
        default:
            return;
    }
}

bool voc_frame(voc_data *d, bool /*force_full_frame*/) {
    if (!d->interlace_enabled() || !host_shr_enabled(d->computer)) {
        return false;
    }
    if (!d->frame || !d->view || !d->computer->mmu) {
        return false;
    }

    uint8_t *ram = d->computer->mmu->get_memory_base();
    const uint32_t msz = d->computer->mmu->get_memory_size();
    if (!ram || msz < MEGAII_E1_SHR + SHR_WINDOW) {
        return false;
    }

    bool e1_interleave = false;
    if (d->computer->cpu && d->computer->cpu->mmu) {
        auto *gs = dynamic_cast<MMU_IIgs *>(d->computer->cpu->mmu);
        if (gs) {
            e1_interleave = gs->is_aux_linear();
        }
    }

    const uint8_t *e0 = ram + MEGAII_E0_SHR;
    const uint8_t *e1 = ram + MEGAII_E1_SHR;

    d->frame->open();
    d->view->generate_voc400(e1, e0, d->frame.get(), e1_interleave, false);
    d->frame->close();

    SDL_Texture *tex = d->frame->get_texture();
    if (tex && d->computer->video_system) {
        SDL_FRect src = {0.0f, 0.0f, 640.0f, 400.0f};
        d->computer->video_system->render_frame(tex, &src, nullptr);
    }

    d->field_parity = !d->field_parity;
    return true;
}

} // namespace

void init_voc(computer_t *computer, SlotType_t slot) {
    voc_data *d = new voc_data();
    d->computer = computer;
    d->_slot = slot;
    d->view = std::make_unique<AppleII_View>(nullptr);

    video_system_t *vs = computer->video_system;
    d->frame = std::make_unique<Frame640x400>(640, 400, vs->renderer, PIXEL_FORMAT);
    if (SDL_Texture *t = d->frame->get_texture()) {
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
    }

    const uint16_t slot_base = static_cast<uint16_t>(0xC080 + (slot * 0x10));
    for (uint16_t off = 0; off < VOC_REG_COUNT; off++) {
        computer->mmu->set_C0XX_read_handler(slot_base + off, {voc_read_c0xx, d});
        computer->mmu->set_C0XX_write_handler(slot_base + off, {voc_write_c0xx, d});
    }

    computer->video_system->register_frame_processor(1, [d, computer](bool force_full_frame) -> bool {
        bool ret = voc_frame(d, force_full_frame);
        if (ret && computer->clock && computer->clock->get_video_scanner()) {
            computer->clock->get_video_scanner()->get_frame_scan()->clear();
        }
        return ret;
    });

    computer->register_reset_handler([d](bool /*cold_start*/) {
        d->reset_regs();
        return true;
    });

    computer->register_shutdown_handler([d]() {
        delete d;
        return true;
    });

    computer->register_debug_display_handler("voc", DH_VOC, [d]() -> DebugFormatter * {
        DebugFormatter *df = new DebugFormatter();
        df->addLine("VOC slot %d", (int)d->_slot);
        df->addLine("  C0B1 Mode     %02X  GGBus=%d MainPageLin=%d SHRSource=%d",
                    d->reg[1],
                    (d->reg[1] & 0x01) != 0,
                    (d->reg[1] & 0x08) != 0,
                    (d->reg[1] >> 4) & 0x03);
        df->addLine("  C0B3 Dissolve %02X", d->reg[3]);
        df->addLine("  C0B4 Key GB   %02X", d->reg[4]);
        df->addLine("  C0B5 Key R    %02X  InterlaceEnable=%d",
                    d->reg[5], (d->reg[5] & 0x80) != 0);
        df->addLine("  C0B6 Hue/Sat  %02X", d->reg[6]);
        df->addLine("  Interlace 640x400: %s  Field: %d",
                    d->interlace_enabled() ? "ON" : "OFF",
                    d->field_parity ? 1 : 0);
        return df;
    });
}
