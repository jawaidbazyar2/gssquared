/*
 *   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright holders: Dirk Best.
 *   See secondsight-video.LICENSE for the BSD-3-Clause terms.
 *
 *   The Brooktree Bt481 is Second Sight's RAMDAC: palette RAM plus digital-
 *   to-analog color conversion. Oak supplies pixel bytes and register accesses.
 *   Indexed pixels select a palette entry; direct-color pixels carry red, green
 *   and blue bits themselves. RGB means red, green and blue; the model returns host colors.
 *   An overlay is a separate four-bit input that can replace the pixel color;
 *   Second Sight's normal rendering supplies zero on those inputs.
 *
 *   OTI087 owns this chip. Register accesses run on the card execution thread;
 *   main-thread rendering reads colors only after SSCard synchronizes. Palette
 *   entries and access counters belong to the chip; VideoColor values are copies.
 *   Sources: Brooktree Bt481A datasheet L481A_C pp. 6-8 (palette and pixel
 *   formats), 14-20 (masks and command registers). The corresponding public
 *   scan is https://imrad.com.ua/userdata/modules/productFiles/JgfPJxY2_BT481AKPJ110.pdf .
 */

#include "bt481.hpp"
#include <algorithm>
#include <iterator>

namespace {
// Extract bit n as zero or one when decoding a documented register field.
constexpr unsigned bit(uint32_t value, unsigned n) {
    return (value >> n) & 1;
}
} // namespace

// Reset command/access sequencing without erasing palette RAM.
void Bt481::reset() {
    cra = 0;
    crb = 0x1e;
    hidden_reads = 0;
    recovery_end = 0;
}

// Load the Command A byte; see the format/port bit table in bt481.hpp
// and Brooktree L481A_C pp. 15-18. The next access uses the new decode.
void Bt481::command_w(uint8_t data) {
    cra = data;
}

// Track power-down recovery when the command register leaves sleep.
// Brooktree Bt481A L481A_C, Command Register B and power-down operation.
void Bt481::command_b_w(uint8_t data) {
    bool was_sleeping = pin_6_8 && (crb & 1);
    crb = data;
    // Re-entering sleep cancels recovery. One second is the model's wake
    // interval; it is not an asserted minimum for the physical Bt481.
    if (pin_6_8 && (crb & 1))
        recovery_end = 0;
    else if (was_sleeping)
        recovery_end = now + 1.0;
}

// Decode VGA-compatible DAC ports and count the hidden command-register
// read sequence; debugger peeks must not advance that sequence.
uint8_t Bt481::vga_r(uint32_t rs, bool effects) {
    rs &= 3;
    if (effects) {
        if (rs == 2 && !bit(cra, 2))
            hidden_reads = std::min<unsigned>(4, hidden_reads + 1);
        else if (hidden_reads < 4)
            hidden_reads = 0;
    }
    if (bit(cra, 2))
        rs |= 4;
    switch (rs) {
        case 0:
        case 3:
            return address_r();
        case 1:
            return palette_r(effects);
        case 2:
            return mask_r();
        case 4:
        case 7:
            return overlay_address_r();
        case 5:
            return overlay_r(effects);
        case 6:
            return command_r();
    }
    return 0xff;
}

// Decode direct/hidden DAC registers, then reset the hidden-read sequence.
// Brooktree Bt481A L481A_C, MPU interface and command-register access.
void Bt481::vga_w(uint32_t rs, uint8_t data) {
    rs &= 3;
    // Four reads of the normal mask port arm a Command A write there. The
    // next write consumes that access window; ordinary palette writes do not
    // leave it armed (Brooktree L481A_C command access, pp. 15-18).
    bool const hidden = rs == 2 && hidden_reads == 4;
    hidden_reads = 0;
    if (hidden || bit(cra, 2))
        rs |= 4;
    switch (rs) {
        case 0:
            address_w(data);
            break;
        case 1:
            palette_w(data);
            break;
        case 2:
            mask_w(data);
            break;
        case 3:
            address_read_w(data);
            break;
        case 4:
            overlay_address_w(data & 15);
            break;
        case 5:
            overlay_w(data);
            break;
        case 6:
            command_w(data);
            break;
        case 7:
            overlay_address_read_w(data & 15);
            break;
    }
}

// Decode Command A pixel packing so Oak advances by the RAMDAC input width.
unsigned Bt481::bytes_per_pixel() const {
    return !bit(cra, 7) ? 1 : !bit(cra, 4) ? 2 : bit(cra, 5) ? 3 : 4;
}

// Account for the RAMDAC input multiplexing when deriving the pixel clock.
unsigned Bt481::pixels_per_clock_divisor() const {
    return !bit(cra, 7) ? 1 : bytes_per_pixel() / (bit(cra, 5) ? 1 : 2);
}

// Apply pixel/overlay masks and the six/eight-bit DAC precision before RGB output.
VideoColor Bt481::pixel_select(uint8_t index, uint8_t overlay) const {
    if (asleep())
        return VideoColor(0, 0, 0);
    if (overlay & overlay_mask)
        return overlay_select(overlay);
    VideoColor const color = get_color(index & read_mask);
    return eight_bit() ? color : VideoColor(color.r() & 0xfc, color.g() & 0xfc, color.b() & 0xfc);
}

// Select one of the overlay palette entries, honoring DAC sleep and precision.
VideoColor Bt481::overlay_select(uint8_t index) const {
    if (asleep())
        return VideoColor(0, 0, 0);
    VideoColor const color = get_color(256 + (index & overlay_mask));
    return eight_bit() ? color : VideoColor(color.r() & 0xfc, color.g() & 0xfc, color.b() & 0xfc);
}

// Decode 15-, 16-, 24- or 32-bit input and optional overlay selection.
// Brooktree Bt481A L481A_C pp. 7-8 and 20.
VideoColor Bt481::direct_color(uint32_t pixel, uint8_t overlay) const {
    if (!bit(cra, 7))
        return pixel_select(pixel, overlay);
    if (asleep())
        return VideoColor(0, 0, 0);
    // L481A_C p20 B6: external OL inputs can override true color. This is
    // separate from the fourth P-bus byte in 32-bit palette-overlay mode.
    if (pin_6_8 && bit(crb, 6) && (overlay & overlay_mask))
        return overlay_select(overlay);
    // L481A_C pp7-8: unused low DAC bits are zero, not replicated.
    if (!bit(cra, 4))
        return bit(cra, 6) ? VideoColor((pixel >> 8) & 0xf8, (pixel >> 3) & 0xfc, (pixel << 3) & 0xf8)
                           : VideoColor((pixel >> 7) & 0xf8, (pixel >> 2) & 0xf8, (pixel << 3) & 0xf8);
    if (!bit(cra, 5) && ((pixel >> 24) & read_mask))
        return pixel_select(pixel >> 24);
    return bit(cra, 1) ? VideoColor(pixel >> 16, pixel >> 8, pixel) : VideoColor(pixel, pixel >> 8, pixel >> 16);
}

// Read the current palette address, including advance caused by read prefetch
// (Brooktree L481A_C p. 6). This does not consume a color component.
uint8_t Bt481::address_r() {
    return addr;
}

// Reset RGB component sequencing; indirect register mode also latches its index.
void Bt481::address_w(uint8_t data) {
    if (bit(cra, 0))
        indirect_index = data & 0x07;

    addr = data;
    addr_rgb = 0;
}

// Return the current RGB component and advance only for a side-effecting read.
uint8_t Bt481::palette_r(bool effects) {
    uint8_t const result = eight_bit() ? color[addr_rgb] : (color[addr_rgb] >> 2);
    if (effects) {
        addr_rgb = (addr_rgb + 1) % 3;
        if (!addr_rgb)
            palette_read_load(0);
    }
    return result;
}

// Commit a palette color after its third component, then advance the palette address.
void Bt481::palette_w(uint8_t data) {
    color[addr_rgb] = eight_bit() ? data : (data << 2);

    // Commit only a complete RGB triplet; partial writes stay staged.
    if (addr_rgb == 2)
        set_color(addr++, VideoColor(color[0], color[1], color[2]));

    addr_rgb = (addr_rgb + 1) % 3;
}

// Multiplex the pixel read mask and indirect registers selected by Command A.
uint8_t Bt481::mask_r() {
    return indirect_read(bit(cra, 0) ? indirect_index : READ_MASK_REGISTER);
}

// Write the mask or indirect command/cursor register selected by Command A.
void Bt481::mask_w(uint8_t data) {
    // Command A bit 0 redirects the mask port to an indirect register;
    // unimplemented selections read as all ones or ignore the write.
    if (bit(cra, 0)) {
        switch (indirect_index) {
            case READ_MASK_REGISTER:

                read_mask = data;
                overlay_mask = overlay_mask_pending;
                break;

            case OVERLAY_MASK_REGISTER:

                // L481A_C p14: latch together with the next pixel-mask write.
                overlay_mask_pending = data & 0x0f;
                break;

            case COMMAND_REGISTER_B:

                command_b_w(data);
                break;

            case CURSOR_REGISTER:

                cursor = data;
                break;

            default:

                break;
        }
    } else {
        read_mask = data;
        overlay_mask = overlay_mask_pending;
    }
}

// Start a palette read at its red component and load the addressed color.
void Bt481::address_read_w(uint8_t data) {
    addr = data;
    addr_rgb = 0;
    palette_read_load(0);
}

// Load the RGB staging bytes, applying the selected DAC precision.
void Bt481::palette_read_load(unsigned base) {
    // L481A_C p6: read addressing prefetches RGB and advances immediately.
    if (base && addr >= 20) {
        // An out-of-range prefetch must not overrun the palette, including
        // the prefetch following the last Bt482 cursor color.
        std::fill(std::begin(color), std::end(color), 0);
        return;
    }
    VideoColor const pixel = get_color(base + addr);
    color[0] = pixel.r();
    color[1] = pixel.g();
    color[2] = pixel.b();
    addr = (base && addr < 16) ? ((addr + 1) & 15) : uint8_t(addr + 1);
}

// Read the same address counter through the overlay register bank
// (Brooktree L481A_C p. 6). No component is consumed.
uint8_t Bt481::overlay_address_r() {
    return addr;
}

// Start an overlay-palette write at the first color component.
void Bt481::overlay_address_w(uint8_t data) {
    addr = data;
    addr_rgb = 0;
}

// Read overlay RGB components without advancing on debugger peeks.
uint8_t Bt481::overlay_r(bool effects) {
    uint8_t const result = eight_bit() ? color[addr_rgb] : (color[addr_rgb] >> 2);
    if (effects) {
        addr_rgb = (addr_rgb + 1) % 3;
        if (!addr_rgb)
            palette_read_load(256);
    }
    return result;
}

// Complete overlay palette colors after three component writes.
void Bt481::overlay_w(uint8_t data) {
    if (addr >= 20) {
        return;
    }

    color[addr_rgb] = eight_bit() ? data : (data << 2);

    if (addr_rgb == 2) {
        // Commit only a complete RGB triplet; partial writes stay staged.
        set_color(256 + addr, VideoColor(color[0], color[1], color[2]));

        if (addr < 16)
            // while accessing the overlay color registers the 4 high bits are ignored
            addr = (addr + 1) & 0x0f;
        else
            // accessing cursor color
            addr++;
    }

    addr_rgb = (addr_rgb + 1) % 3;
}

// Select the overlay read address and preload its RGB staging bytes.
void Bt481::overlay_address_read_w(uint8_t data) {
    addr = data;
    addr_rgb = 0;
    // Bt482 cursor RAM uses a byte address, without RGB palette prefetch.
    if (!bit(cursor, 3))
        palette_read_load(256);
}
