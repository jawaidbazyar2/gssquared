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
 *   main-thread rendering reads colors while SSCard has stopped execution. Palette
 *   entries and access counters belong to the chip; VideoColor values are copies.
 *   Sources: Brooktree Bt481A datasheet L481A_C pp. 6-8 (palette and pixel
 *   formats), 14-20 (masks and command registers). The corresponding public
 *   scan is https://imrad.com.ua/userdata/modules/productFiles/JgfPJxY2_BT481AKPJ110.pdf .
 */

#pragma once
#include <cstdint>
#include <array>

// Value-type host color: eight bits each for alpha, red, green and blue.
// Owned by the containing palette or caller; it has no thread or chip state.
struct VideoColor {
    uint32_t value;
    // Wrap an already packed ARGB word; default is opaque black.
    constexpr VideoColor(uint32_t value = 0xff000000) : value(value) {}
    // Pack three eight-bit components with opaque alpha ($FF).
    constexpr VideoColor(uint8_t r, uint8_t g, uint8_t b)
        : value(0xff000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b) {}
    // Extract the red component, 0-255.
    constexpr uint8_t r() const { return value >> 16; }
    // Extract the green component, 0-255.
    constexpr uint8_t g() const { return value >> 8; }
    // Extract the blue component, 0-255.
    constexpr uint8_t b() const { return value; }
    // Return the packed word expected by the host pixel buffer.
    constexpr operator uint32_t() const { return value; }
};

// RAMDAC register/palette state shared by Video Graphics Array (VGA)
// register I/O and pixel conversion.
class Bt481 {
public:
    // Reset control/access state without erasing palette RAM.
    void reset();
    // Supply absolute emulated seconds for sleep/wake recovery.
    void advance(double seconds) { now = seconds; }
    // Read register-select pins 0-3; effects=false preserves access sequencing.
    uint8_t vga_r(uint32_t rs, bool effects = true);
    // Write data through register-select pins 0-3, including hidden command access.
    void vga_w(uint32_t rs, uint8_t data);
    // Palette RAM changes at the blue component; mask/command writes can
    // change output immediately. Address and staging writes need no raster work.
    bool display_write(uint8_t rs) const {
        return (rs & 3) == 2 || ((rs & 3) == 1 && addr_rgb == 2);
    }
    // Read Command A without changing the hidden-access counter.
    uint8_t command_r() const { return cra; }
    // Debugger copies of the masks (Brooktree L481A_C pp. 14-20).
    // These bypass the indirect-address and hidden-command read sequences.
    uint8_t pixel_mask() const { return indirect_read(0); }
    uint8_t overlay_pixel_mask() const { return overlay_mask; }
    // Decode an explicit indirect index without selecting it or consuming DAC
    // data. Both the mask port and passive observers use this map (L481A_C p. 14).
    uint8_t indirect_read(unsigned offset) const {
        switch (offset) {
            case READ_MASK_REGISTER: return read_mask;
            case OVERLAY_MASK_REGISTER: return overlay_mask_pending;
            case COMMAND_REGISTER_B: return crb;
            case CURSOR_REGISTER: return cursor;
            default: return 0xff;
        }
    }
    // Caller-owned copy of register-access sequencing for passive inspection.
    // Read on the main thread while card execution is stopped; retaining the
    // copy does not reserve or modify the live chip.
    // L481A_C pp. 14-20 describes the shared address and component sequencing.
    struct AccessState {
        uint8_t pending_overlay_mask, address, component;
        std::array<uint8_t, 3> staged_color;
        uint8_t indirect_index, hidden_reads;
        bool pin_6_8;
        uint8_t cursor;
    };
    // Snapshot access counters/staged bytes without consuming a component or
    // committing a pending mask.
    AccessState access_state() const {
        return {indirect_read(1), addr, addr_rgb, {color[0], color[1], color[2]},
                indirect_index, hidden_reads, pin_6_8, indirect_read(3)};
    }
    // Number of Oak input bytes consumed by each pixel in the selected format.
    unsigned bytes_per_pixel() const;
    // Number of input clocks per output pixel for Oak timing conversion.
    unsigned pixels_per_clock_divisor() const;
    // Resolve an indexed byte plus four overlay pins to a masked output color.
    VideoColor pixel_select(uint8_t index, uint8_t overlay = 0) const;
    // Resolve the overlay palette index, honoring sleep and component precision.
    VideoColor overlay_select(uint8_t index) const;
    // Decode the packed input word in Command A format; overlay can override it.
    VideoColor direct_color(uint32_t pixel, uint8_t overlay = 0) const;
    // True while power-down is selected or modelled wake recovery is pending.
    bool asleep() const { return (pin_6_8 && (crb & 1)) || now < recovery_end; }

    // Read the palette address counter, which may already point past a prefetched color.
    uint8_t address_r();
    // Select an indexed write address and restart at its red component.
    void address_w(uint8_t data);
    // Read the staged red/green/blue component; effects=false does not advance.
    uint8_t palette_r(bool effects = true);
    // Write one component; the blue write commits the color and advances the address.
    void palette_w(uint8_t data);
    // Read pixel mask or the indirect register selected by Command A bit 0.
    uint8_t mask_r();
    // Write the selected mask/control; pixel-mask writes commit pending overlay masks.
    void mask_w(uint8_t data);
    // Select a read address, prefetch its color and advance the address counter.
    void address_read_w(uint8_t data);
    // Read the shared palette/overlay address counter.
    uint8_t overlay_address_r();
    // Select an overlay write address and restart component sequencing.
    void overlay_address_w(uint8_t data);
    // Read a staged overlay component, optionally advancing to the next one.
    uint8_t overlay_r(bool effects = true);
    // Write an overlay component; commit at blue and wrap addresses 0-15.
    void overlay_w(uint8_t data);
    // Select an overlay read address and prefetch unless cursor-RAM mode is selected.
    void overlay_address_read_w(uint8_t data);
    // Load Command A; format changes affect subsequent pixels and register decode.
    void command_w(uint8_t data);
    // Direct palette inspection by index; bypasses hardware read sequencing.
    VideoColor get_color(unsigned index) const { return palette.at(index); }
    // Direct palette replacement by index; bypasses hardware component sequencing.
    void set_color(unsigned index, VideoColor color) { palette.at(index) = color; }

private:
    // Indirect indices (L481A_C pp. 14-20): 0 masks pixel index bits;
    // 1 stages the four overlay mask bits; 2 is Command B; 3 stores cursor
    // control retained for Bt482-compatible register access.
    // Command A: bit 7 direct color, 6 16-bit vs 15-bit, 5 three-byte vs
    // multiplexed input, 4 24/32-bit vs 15/16-bit, 2 extended register select,
    // 1 RGB byte order, 0 indirect-register select. Bit 3 is retained here.
    // Command B: bit 0 sleep, 1 eight-bit palette, 6 overlay in direct color;
    // other bits are retained, including analog sync controls not rendered.
    static constexpr int READ_MASK_REGISTER = 0, OVERLAY_MASK_REGISTER = 1;
    static constexpr int COMMAND_REGISTER_B = 2, CURSOR_REGISTER = 3;
    uint8_t cra = 0, crb = 0x1e;
    // Staging buffer holds one RGB triplet; addr_rgb selects red=0, green=1, blue=2.
    uint8_t color[3] = {};
    uint8_t addr = 0, addr_rgb = 0, indirect_index = 0;
    uint8_t read_mask = 0xff, overlay_mask = 0x0f, overlay_mask_pending = 0x0f;
    uint8_t cursor = 0, hidden_reads = 0;
    bool pin_6_8 = true;
    double now = 0, recovery_end = 0;
    // 256 pixel colors, 16 overlays and four compatible cursor colors.
    std::array<VideoColor, 276> palette{};
    // Eight-bit precision requires both the physical pin and Command B bit 1.
    bool eight_bit() const { return pin_6_8 && (crb & 2); }
    // Prefetch RGB at addr from palette base 0 or overlay base 256; advance addr.
    void palette_read_load(unsigned base);
    // Store Command B and begin modelled recovery when leaving power-down.
    void command_b_w(uint8_t data);
};
