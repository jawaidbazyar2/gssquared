/*
 *   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright holders: Nathan Woods, Peter Trauner, Angelo Salese.
 *   See secondsight-video.LICENSE for the BSD-3-Clause terms.
 *
 *   The Oak OTI-087X is Second Sight's VGA (Video Graphics Array) controller.
 *   It owns one MiB of video RAM (VRAM), decodes Z180 memory/I/O accesses and
 *   produces a raster: pixels scanned left to right, then top to bottom.
 *   The CRTC (cathode-ray-tube controller) counts pixels/lines and generates
 *   blanking intervals, when the monitor must not display video. The RAMDAC
 *   is the Brooktree Bt481 palette memory and digital-to-analog converter;
 *   the model returns RGB (red, green, blue) colors rather than voltages.
 *
 *   SSCard owns OTI087. Its execution thread changes registers and advances
 *   time. The main thread reads it while card execution is stopped, either
 *   after synchronization or after claiming a completed scan for presentation.
 *   A latch retains a value for later use; register writes can prepare a new
 *   screen address before the display latches it. An event is a scheduled
 *   change such as the start of vertical blanking. Time is in emulated seconds;
 *   an attosecond is 10^-18 seconds, used to round clock periods consistently.
 *
 *   Sources: Oak OTI-087 February 1993 databook pp. 29-46 and OTI-087X addendum
 *   (https://bitsavers.org/components/oakTechnology/OTI-087_Feb1993.pdf);
 *   IBM VGA/XGA Technical Reference May 1992, VGA registers; Brooktree Bt481A
 *   L481A_C pp. 6-8, 14-20 for palette and color formats.
 *   Wide-latch analysis: https://github.com/Webifi/second-sight-reference,
 *   docs/15-oak-drawing.md and docs/reference/oti087-registers.md.
 */

#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include "bt481.hpp"
#include "ss_eeprom.hpp"

struct video_system_t;
struct SDL_Texture;

// Pixel rectangle passed by value on the rendering thread. Inclusive edges
// match the CRTC last-pixel/last-line convention; inverted bounds are empty.
struct ss_video_rect_t {
    int left, right, top, bottom;
    // Number of included columns; use empty() before treating it as a size.
    int width() const { return right - left + 1; }
    // Number of included rows; use empty() before treating it as a size.
    int height() const { return bottom - top + 1; }
    // True when either pair of inclusive edges is inverted.
    bool empty() const { return right < left || bottom < top; }
    // Whether screen pixel (x,y) lies inside these inclusive edges.
    bool contains(int x, int y) const { return x >= left && x <= right && y >= top && y <= bottom; }
    // Return the overlap with b; an inverted result represents no overlap.
    ss_video_rect_t intersect(ss_video_rect_t b) const {
        return {std::max(left, b.left), std::min(right, b.right), std::max(top, b.top), std::min(bottom, b.bottom)};
    }
};

// Borrowed host surface used only by the rendering thread; pitch counts
// uint32_t pixels between rows, not bytes. The caller owns the allocation.
struct ss_video_buffer_t {
    uint32_t *pixels;
    int width, height, pitch; // pitch in uint32_t pixels
    // Full drawable rectangle of this borrowed buffer.
    ss_video_rect_t bounds() const { return {0, width - 1, 0, height - 1}; }
    // Borrow the pixel at row y, column x; callers clip coordinates first.
    uint32_t &pixel(int y, int x) { return pixels[y * pitch + x]; }
    // Paint area in opaque host color, clipping it to this buffer.
    void fill(uint32_t color, ss_video_rect_t area) {
        area = area.intersect(bounds());
        if (area.empty())
            return;
        for (int y = area.top; y <= area.bottom; y++)
            std::fill(pixels + y * pitch + area.left, pixels + y * pitch + area.right + 1, color);
    }
};

// Geometry/timing owned by OTI087 and borrowed by synchronized presentation.
// Totals include blanking; periods use integer attosecond rounding.
struct ss_video_timing_t {
    int width = 640, height = 480, total_width = 800, total_height = 525;
    uint32_t pixel_clock = 25175000;
    unsigned pixel_divisor = 1;
    // CRTC (cathode-ray tube controller) character counts before conversion to
    // integer texture pixels. Their ratio is active line time / total line time:
    // character dots, clock selection and dividers cancel. Keep the counts so
    // three-byte RAMDAC pixels do not round the monitor's horizontal geometry.
    unsigned display_characters = 80, line_characters = 100;
    // Retain diagnostic clock inputs with the accepted raster, not partially
    // programmed registers. Vertical counts above always include both fields.
    uint32_t oscillator_clock = 25175000;
    bool interlaced = false, sequencer_half_clock = false, board_half_clock = false;
    // Round the pixel period to whole attoseconds before multiplying by the
    // raster totals, consistently with the other video timing calculations.
    uint64_t frame_units() const {
        uint64_t ticks = uint64_t(total_width) * total_height * pixel_divisor;
        return (1000000000000000000ULL / pixel_clock) * ticks;
    }
    // Duration of one complete raster in seconds, including blanking.
    double frame_period() const { return double(frame_units()) * 1e-18; }
    // Duration of one complete raster row in seconds.
    double line_period() const { return double(frame_units() / total_height) * 1e-18; }
    // Duration of one output pixel position in seconds.
    double pixel_period() const { return double(frame_units() / (total_height * total_width)) * 1e-18; }
    // Active video rectangle; raster totals also include blank positions.
    ss_video_rect_t visible() const { return {0, width - 1, 0, height - 1}; }
};

enum class ss_video_mode_t { Text, Planar, VGA256, Packed, CGA };
// Caller-owned copy of mode, geometry and palette; safe to retain after the
// synchronized read because it contains no pointers into the card.
struct ss_video_frame_t {
    ss_video_mode_t mode;
    int width, height;
    unsigned pitch, start, depth; // packed: byte units; legacy: VGA offset/address units
    uint8_t palette[256][3];
};
// Value returned to SSCard on the execution thread: one byte plus wait
// periods measured at 18 MHz. SSCard converts the delay to its current clock.
struct ss_video_access_t {
    uint8_t data;
    unsigned waits;
};

// Owns VGA registers, one MiB of VRAM, the Bt481 and the serial EEPROM;
// the board supplies absolute time before accessing these devices.
class OTI087 {
public:
    static constexpr int MAX_FRAME_DIMENSION = 4096;
    static constexpr uint32_t VRAM_SIZE = 0x100000;
    // Allocate one MiB of VRAM and initialize deterministic power-up state.
    OTI087();
    // Reset registers and pending chip events; retain VRAM and EEPROM contents.
    void reset();
    // Absolute emulated seconds; call before bus accesses and at host frame end.
    void advance(double seconds);
    // Absolute emulated seconds last supplied by the board.
    double time() const { return now; }
    // Earliest pending chip transition in absolute seconds; infinity means none.
    double next_event() const;
    // Current horizontal output-pixel position, including blanking.
    int hpos() const;
    // Current raster row, including blanking and both interlaced fields.
    int vpos() const;
    // Count of frames completed, used by cursor and text blink logic.
    uint64_t frame_number() const { return frames; }
    // Positive seconds until the next occurrence of output position (x,y).
    double time_until_pos(int y, int x = 0) const;
    // Borrow geometry/timing; synchronize card execution before host inspection.
    const ss_video_timing_t &display_timing() const { return timing; }
    unsigned odd_field_sync_phase() const;
    // Copy active mode, addressing units and resolved palette for inspection.
    ss_video_frame_t display_state() const;

    // Read the 16-bit port; effects=false peeks without consuming register state.
    uint8_t io_r(uint32_t port, bool effects = true);
    // Whether this enabled chip drives the specified port read.
    bool io_read_selected(uint16_t port) const;
    // Whether this board-supplied physical byte address selects video memory.
    bool memory_read_selected(uint32_t address) const {
        uint32_t offset;
        return decode_memory(address, false, offset);
    }
    // Write data to an indexed VGA/Oak port, applying its register side effects.
    void io_w(uint32_t port, uint8_t data);
    // Read a byte and report wait periods in units of a fixed 18 MHz clock.
    ss_video_access_t memory_read(uint32_t address);
    // Store data and return wait periods in units of a fixed 18 MHz clock.
    unsigned memory_write(uint32_t address, uint8_t data);
    // Decoded memory read with latch effects; unselected addresses return $FF.
    uint8_t bus_memory_r(uint32_t address);
    // Decoded memory write with plane/mask effects; unselected addresses do nothing.
    void bus_memory_w(uint32_t address, uint8_t data);
    // Convenience read using an already translated Oak address.
    uint8_t memory_r(uint32_t address) { return bus_memory_r(address); }
    // Convenience write using an already translated Oak address.
    void memory_w(uint32_t address, uint8_t data) { bus_memory_w(address, data); }
    // Read through the board window; half chooses its upper 512 KiB.
    uint8_t memory_r(uint32_t address, bool half) { return bus_memory_r(card_address(address, half)); }
    // Write through the board window with the same half-select translation.
    void memory_w(uint32_t address, bool half, uint8_t data) { bus_memory_w(card_address(address, half), data); }
    // Raw display-byte inspection, bypassing CPU aperture and latch effects.
    uint8_t mem_linear_r(uint32_t address) const { return linear_byte(address); }
    // Raw display-byte store, bypassing CPU aperture and latch effects.
    void mem_linear_w(uint32_t address, uint8_t data) { linear_byte_w(address, data); }

    // Apply the board output-clock divider and recompute raster timing.
    void output_divider_w(int state) {
        output_divider = bool(state);
        recompute_params();
    }
    // Cable sense: nonzero=AppleColor RGB, zero=VGA; read by mode-setting firmware.
    void monitor_type_w(int applecolor) { monitor_override = !applecolor; }
    // Selected pixel oscillator frequency in Hz (Oak OTI-068 frequency table p. 7).
    uint32_t vclk_hz() const;
    // Read the write cache enable, extended $22 bit 3 (databook p. 42).
    bool write_cache_enabled() const { return ext[0x22] & 8; }
    // An optional longer access delay also holds IOCHRDY for that duration.
    void set_access_wait(double seconds) { access_delay = std::max(seconds, 0.0); }
    // The Oak's command-width floor in seconds: two read and three write waits
    // at 18 MHz (OTI-087 p47). SSCard::extra_wait charges the board's longer count.
    double access_wait(bool write = false) const {
        return std::max(access_delay, double(write ? 3 : 2) / 18000000);
    }
    // Whether the enabled vertical interrupt latch requests service.
    bool cint_r() const { return crtc.irq_latch && !crtc.irq_disable; }
    // Electrical vertical-sync output after Oak polarity/test controls.
    int vsync_r();
    // Horizontal sync, including CRTC skew and Miscellaneous Output polarity.
    int hsync_r() const;
    // Characters or lines of a CRTC retrace pulse. It starts where the counter
    // equals start and ends where the low bits (mask) of the counter equal
    // end; the counter wraps at total first (IBM VGA/XGA TRM pp. 2-60, 2-71).
    // 0 when the counter never matches end, so the pulse never ends.
    static int retrace_length(int start, int end, int mask, int total) {
        for (int n = 1; n < total; ++n)
            if ((((start + n) % total) & mask) == end) return n;
        return 0;
    }
    // Borrow the board's retained SD7:0 byte; the owner outlives this chip.
    // Scan-test convention: second-sight-reference
    // docs/reference/oti087-registers.md, extended register $03.
    void bind_system_data(const uint8_t &data) { system_data = &data; }
    // Electrical output is CINTn; true here means asserted (low at the pin).
    std::function<void(bool)> cint_callback;
    std::function<void(bool)> ready_callback;
    std::function<void(bool, bool, bool)> eeprom_callback; // DI, CS, SK observation
    // Borrow the palette/converter owned by this chip; caller must synchronize.
    Bt481 &dac() { return ramdac; }
    // Passive inspection borrows chip state only after the card is synchronized.
    const Bt481 &dac() const { return ramdac; }
    // Resolve the eight-bit overscan (border) palette index through the Bt481.
    VideoColor border_color() const { return ramdac.pixel_select(attribute.data[0x11]); }
    // Borrow persistent configuration memory for main-thread load/save.
    SerialEEPROM93C46 &serial_eeprom() { return eeprom; }

    // Native ARGB output. Rendering is deferred to frame end. A start-address
    // latch change records a row boundary, never a timer for every scanline.
    // Draw the clipped region into borrowed pixels; do not advance chip time.
    void render(ss_video_buffer_t surface, ss_video_rect_t clip);
    // Draw all rows with the display-address changes recorded for this frame.
    void render_frame(ss_video_buffer_t surface);
    // Copy changed rows to caller-owned SDL texture; true means upload succeeded.
    // An optional host CRT filter receives completed pixels, never chip state.
    // odd_texture retains the other field when the output look separates fields.
    bool upload_frame(SDL_Texture *texture, bool full = false,
        class SecondSightCrt *crt = nullptr, SDL_Texture *odd_texture = nullptr);
    // Read-only interlace flag for host field presentation (Oak $14 bit 7).
    bool display_interlaced() const { return get_interlace_mode(); }

private:
    uint8_t miscellaneous_output;
    uint8_t feature_control;

    // Sequencer registers: plane-write enables, font banks and pixel-clock division.
    struct sequencer_t {
        uint8_t index;
        uint8_t data[0x100];
        uint8_t map_mask;
        uint32_t address_mask;
        // Font-bank selectors used by text attribute bit 3.
        struct {
            uint8_t A, B;
        } char_sel;
    } sequencer;

    // CRTC counters and display-address latches, including split-screen and IRQ state.
    struct crtc_t {
        uint8_t index;
        uint8_t data[0x100];
        uint16_t horz_total;
        uint16_t horz_disp_end;
        uint8_t horz_blank_start;
        uint8_t horz_blank_end;
        uint8_t horz_retrace_start;
        uint8_t horz_retrace_skew;
        uint8_t horz_retrace_end;
        uint8_t disp_enable_skew;
        uint8_t evra;
        uint16_t vert_total;
        uint16_t vert_disp_end;
        uint16_t vert_retrace_start;
        uint8_t vert_retrace_end;
        uint16_t vert_blank_start;
        uint16_t line_compare;
        uint32_t cursor_addr;
        uint8_t byte_panning;
        uint8_t preset_row_scan;
        uint8_t scan_doubling;
        uint8_t maximum_scan_line;
        uint8_t cursor_enable;
        uint8_t cursor_scan_start;
        uint8_t cursor_skew;
        uint8_t cursor_scan_end;
        uint32_t start_addr;
        uint32_t start_addr_latch;
        uint8_t protect_enable;
        uint8_t bandwidth;
        uint16_t offset;
        uint8_t word_mode;
        uint8_t dw;
        uint8_t div4;
        uint8_t underline_loc;
        uint16_t vert_blank_end;
        uint8_t sync_en;
        uint8_t aw;
        uint8_t div2;
        uint8_t sldiv;
        uint8_t map14;
        uint8_t map13;
        uint8_t irq_clear;
        uint8_t irq_disable;
        uint8_t irq_latch;
        uint8_t no_wrap;
    } crtc;

    // Graphics controller: read latches and CPU write/compare/mask settings.
    struct graphics_t {
        uint8_t index;
        uint8_t latch[4];
        uint8_t set_reset;
        uint8_t enable_set_reset;
        uint8_t color_compare;
        uint8_t logical_op;
        uint8_t rotate_count;
        uint8_t shift256;
        uint8_t shift_reg;
        uint8_t read_map_sel;
        uint8_t read_mode;
        uint8_t write_mode;
        uint8_t color_dont_care;
        uint8_t bit_mask;
        uint8_t alpha_dis;
        uint8_t memory_map_sel;
        uint8_t host_oe;
        uint8_t chain_oe;
        bool plain_write;
    } gc;

    // Attribute controller: 16-color lookup, blink, pixel panning and index/data phase.
    struct attribute_t {
        uint8_t index, data[0x15];
        int state;
        uint8_t prot_bit;
        uint8_t pel_shift;
    } attribute;

    std::vector<uint8_t> memory;
    // Aperture decode depends on register writes, not on each CPU byte.
    uint32_t memory_base = 0, memory_size = 0, bank_window = 0;
    uint32_t memory_bank[2] = {};
    uint8_t ext[0x80] = {}, latch[8] = {};
    uint8_t index = 0, setup = 0, enable = 0;
    const uint8_t *system_data = nullptr;
    uint8_t test_data = 0xff;
    bool packed_scanout = false;
    bool rendering_snapshot = false;
    bool output_divider = false, side_effects = true;
    int monitor_override = -1;
    Bt481 ramdac;
    SerialEEPROM93C46 eeprom;
    ss_video_timing_t timing;
    double now = 0, blank_origin = 0, next_frame = 0, next_blank = 0, next_line_zero = 0, next_latch = 0;
    double next_interrupt = 0, next_raster = 0;
    double ready_until = 0, access_delay = 0;
    uint64_t frames = 0;
    // Display address from a latch edge at this raster line. Owned by
    // OTI087; execution records it, synchronized rendering consumes it.
    struct start_change_t {
        int line;
        uint32_t address;
    };
    std::vector<start_change_t> starts;
    // Display controls saved before a visible register write. OTI087 owns the
    // copies; execution records them and the main thread replays them while
    // the card is stopped. Video memory and clock deadlines are not copied.
    struct display_registers_t {
        sequencer_t sequencer;
        crtc_t crtc;
        graphics_t gc;
        attribute_t attribute;
        std::array<uint8_t, 128> ext;
        Bt481 dac;
        uint8_t setup, enable, test_data;
    };
    // End of a span drawn with the saved controls. Position counts output
    // pixels in beam order, including blanking and both interlaced fields.
    struct display_change_t {
        int position;
        display_registers_t registers;
    };
    std::vector<display_change_t> display_changes;
    // Derived colors live across the rows of one register span. Raster replay
    // invalidates them at a state boundary, not for each line or pixel.
    std::array<uint32_t, 256> render_dac_colors{};
    std::array<uint32_t, 16> render_attribute_colors{};
    bool render_colors_valid = false;
    // Address generator at the start of a displayed row. The renderer owns
    // this temporary value. Address is in CRTC counter units, scan is the
    // glyph/plane row, repeat counts duplicate scans, and split marks the
    // region below line compare, where the address counter restarts at zero.
    // Pan is attribute $13 at the start of the line: IBM p2-95 gives it no
    // retrace latch, so a write shows from the next line.
    struct scan_row_t {
        uint32_t address;
        unsigned scan, repeat;
        bool split;
        uint8_t pan;
    };
    // Borrowed only during render_changes; null lets each row derive its address.
    const scan_row_t *render_row = nullptr;
    // Copy display controls, including the DAC palette and sampled test byte.
    display_registers_t display_registers() const;
    // Install replay controls without changing time; invalidate derived colors.
    void display_registers(const display_registers_t &state);
    // Save the pre-write controls through the current beam position.
    void record_display_change();
    // Draw saved spans into the caller's surface, then restore live controls.
    void render_changes(ss_video_buffer_t surface);
    // Return address state for texture row y, weaving fields when interlaced.
    scan_row_t scan_row(unsigned y) const;
    // Advance past field_line within one field, applying row repeats and split.
    void next_scan_row(scan_row_t &row, unsigned field_line) const;
    // Map a CRTC counter and glyph row to a byte address within a VGA plane.
    uint32_t display_address(uint32_t counter, unsigned row_scan) const;
    // Fetch byte for character-clock position; shifts encode counter division
    // and bytes loaded together as powers of two. See IBM pp. 2-49/50, 2-75.
    uint32_t serializer_address(const scan_row_t &row, unsigned character, unsigned count_shift,
                                unsigned load_shift) const;
    // Return log2 of the modeled group loaded for serial pixel output.
    unsigned serializer_load_shift() const;
    // Cache DAC and attribute colors once per register span.
    void prepare_render_colors();
    // Return leftward pixel displacement; split permits the line-compare reset.
    unsigned horizontal_pan(const scan_row_t &row) const;
    // Scan-line division doubles counter durations; total includes two encoded
    // trailing lines (IBM CRTC vertical-total and mode-control registers).
    unsigned vertical_divisor() const { return crtc.sldiv ? 2 : 1; }
    unsigned field_total() const { return (crtc.vert_total + 2) * vertical_divisor(); }
    // Sequencer 1 bit 0 selects eight or nine dots per character, including
    // graphics. IBM p. 2-50 recommends eight dots in graphics; a nine-dot
    // mode can display 360 pixels with a shorter total at a similar line rate,
    // as in Sequential Systems' SECOND/MISC/XXX disk mode file.
    unsigned char_dots() const { return (sequencer.data[1] & 1) ? 8 : 9; }
    // Interlaced texture rows are woven, but the beam visits a complete
    // even field followed by a complete odd field, including both blanks.
    unsigned active_end_line() const {
        return timing.interlaced ? (timing.total_height + timing.height) / 2 : timing.height;
    }
    void vertical_interrupt();
    // Raster deadlines change only at a raster edge or a timing-register
    // write. CPU accesses need only their cached minimum, plus READY/EEPROM.
    void update_raster_deadline();
    std::vector<uint32_t> frame_pixels;
    // Derived display data only. Compare source bytes and DAC output before
    // reusing a row; register writes and debug captures need no dirty hooks.
    std::vector<uint8_t> packed_source, packed_row;
    // Constant-register frames reuse row addresses. Keep every dependency so
    // pitch, split, preset and interlace changes invalidate this derived data.
    std::vector<uint32_t> packed_addresses;
    std::array<uint32_t, 10> packed_address_state{};
    bool packed_addresses_valid = false;
    bool packed_rows_in_memory = true;
    std::array<uint32_t, 256> packed_palette{};
    int packed_width = 0, packed_height = 0;
    uint8_t packed_command = 0;
    bool packed_valid = false, packed_asleep = false;
    int present_top = 0, present_bottom = -1;
    int present_width = 0, present_height = 0;

    // Return whether Oak setup/enable bits permit VGA register and memory access (databook pp. 29-30).
    bool enabled() const;
    // Decode port and return its register byte; side_effects controls destructive reads.
    uint8_t io_read_impl(uint32_t port);
    // Decode programmed counters/clocks into raster geometry and event times
    // (Oak pp. 39-40; IBM VGA/XGA Technical Reference, CRTC registers).
    void recompute_params();
    // Schedule the edge where the programmed start address becomes active (Oak pp. 39-40).
    void schedule_display_latch();
    // Copy the programmed start address into the active value and record the affected raster row.
    void display_latch();
    // Lower readiness for the modelled memory delay; write selects read or write latency.
    void access_begin(bool write);
    // Drive serial EEPROM data/select/clock from extended $18 and report the pin levels (Oak p. 41).
    void eeprom_update();
    // Return oscillator-select bits after board wiring and extended-register decoding (Oak pp. 40-41).
    uint8_t csel_pins() const;
    // Return how many output pixel positions each CRTC character count spans.
    unsigned horizontal_multiplier() const;
    // Return the selected clock-to-output-pixel divisor, including board clock division.
    unsigned pixel_divisor() const;
    // Form the active screen address from VGA low bits and Oak extension bits (Oak pp. 39-40).
    uint32_t latch_start_addr();
    // Oak $14 bit 7 selects alternate even/odd-line fields (databook pp. 39-40).
    bool get_interlace_mode() const { return ext[0x14] & 0x80; }
    // Return whether the raster is inside the programmed vertical-sync interval, including wrap.
    bool vertical_retrace();
    // Miscellaneous Output bit 0 chooses monochrome $3B0 or color $3D0 ports.
    uint16_t get_crtc_port() const { return miscellaneous_output & 1 ? 0x3d0 : 0x3b0; }
    // Share the extended decoder; const Video removes I/O effects at compile
    // time, while mutable Video retains the original port behavior and cost.
    template<class Video> static uint8_t extended_read_impl(Video &video, unsigned offset);
    // Read the Oak register selected by index; status/EEPROM bits reflect live pins (Oak pp. 31-46).
    uint8_t extended_r();
    // Write data to the selected Oak register with its mask and side effects (Oak pp. 31-46).
    void extended_w(uint8_t data);
    // Forward register-select pins to the Bt481, honoring debugger peek mode.
    uint8_t dac_r(uint8_t rs) { return ramdac.vga_r(rs, side_effects); }
    // Forward a VGA DAC write and invalidate cached colors used by scanout.
    void dac_w(uint8_t rs, uint8_t data) {
        if (ramdac.display_write(rs))
            record_display_change();
        uint8_t command = ramdac.command_r();
        ramdac.vga_w(rs, data);
        if (command != ramdac.command_r())
            recompute_params();
    }
    // Apply data to the selected CRTC counter/control field and retain register protection rules.
    void crtc_write(uint8_t data);
    // Apply data to the selected graphics plane, latch, compare or mask field.
    void gc_write(uint8_t data);
    // Use the index/data toggle to interpret data; status reads reset that toggle (IBM VGA attribute registers).
    void attribute_write(uint8_t data);
    // Store data in the selected sequencer register and decode plane/font selections.
    void sequencer_write(uint8_t data);
    // Return the programmed row stride in the addressing units of the selected mode.
    uint16_t offset() const;
    // Return the byte produced for plane 0-3 by the selected VGA write mode,
    // using CPU data, retained read latch and write masks (IBM VGA graphics registers).
    uint8_t vga_latch_write(int plane, uint8_t data);
    // Rotate CPU write data by graphics-controller count, wrapping the byte.
    uint8_t rotate_right(uint8_t value) const { return (value >> gc.rotate_count) | (value << (8 - gc.rotate_count)); }
    // Oak $21 bits 3:2 = 01 selects packed pixels (databook p. 42).
    bool packed() const { return (ext[0x21] & 12) == 4; }
    bool scan_test() const { return (ext[3] & 0x87) == 0x85; }
    // Combine the board half-select with the Z180 VGA aperture address.
    uint32_t card_address(uint32_t address, bool half) const {
        return (address & 0x7ffff) | (half ? 0x80000 : 0);
    }
    // Test whether address selects the configured aperture. On true, write
    // the translated offset; write selects read versus write bank (Oak pp. 42-46).
    bool decode_memory(uint32_t address, bool write, uint32_t &offset) const;
    void update_memory_mapping();
    // Convert a linear display address to the four-plane backing-store layout.
    uint32_t storage_address(uint32_t address) const;
    // Return a raw linear display byte after storage-address translation; no CPU latch effects.
    uint8_t linear_byte(uint32_t address) const;
    // Store data at a raw linear display address after storage translation.
    void linear_byte_w(uint32_t address, uint8_t data);
    // Translate chain-four addressing: the low two address bits choose a VGA plane.
    uint32_t chain4_address(uint32_t address) const;
    // Read address into all plane latches, then return the selected plane or
    // color-compare result (IBM VGA graphics-controller read modes).
    uint8_t planar_r(uint32_t address);
    // Apply a CPU byte write at address using map mask, write mode and retained latches.
    void planar_w(uint32_t address, uint8_t data);
    // Apply the Oak packed-pixel write/expansion mode at address using byte data (Oak pp. 42-46).
    void packed_w(uint32_t address, uint8_t data);
    // Apply IBM VGA write equations to the selected saved latch byte. Oak
    // wide-latch use is inferred (second-sight-reference
    // docs/15-oak-drawing.md, "The latch: copying 4 pixels per access").
    uint8_t vga_write_byte(unsigned plane, uint8_t data, uint8_t saved) const {
        auto bit = [](unsigned value, unsigned n) { return (value >> n) & 1; };
        if (gc.plain_write)
            return data;
        uint8_t mask = gc.bit_mask;
        switch (gc.write_mode) {
            case 0:
                data = bit(gc.enable_set_reset, plane) ? (bit(gc.set_reset, plane) ? 0xff : 0) : rotate_right(data);
                break;
            case 1:
                return saved;
            case 2:
                data = bit(data, plane) ? 0xff : 0;
                break;
            case 3:
                mask &= rotate_right(data);
                data = bit(gc.set_reset, plane) ? 0xff : 0;
                break;
        }
        switch (gc.logical_op) {
            case 1: data &= saved; break;
            case 2: data |= saved; break;
            case 3: data ^= saved; break;
        }
        return (data & mask) | (saved & ~mask);
    }
    // Render clipped packed pixels into borrowed surface using Bt481 color conversion.
    void draw_packed(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Render packed rows with source/palette reuse; return false when another
    // mode or frame change requires the general renderer. surface remains caller-owned.
    bool render_packed_frame(ss_video_buffer_t surface);
    // Render clipped character cells, font/attribute bytes and text cursor into surface.
    template<bool Test> void draw_text(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    void draw_scan_test(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Combine one bit from each of four planes into a 16-color pixel in surface.
    // Nine adds the zero ninth dot of a nine-dot character clock.
    template<bool Nine> void draw_planar(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Render chained/unchained VGA 256-color bytes through the palette into surface.
    template<bool Nine> void draw_vga256(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Render the legacy Color Graphics Adapter compatible two-bit pixel organization.
    template<bool Nine> void draw_cga(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Overlay the Oak two-plane hardware cursor within clip (Oak pp. 34-36).
    void draw_cursor(ss_video_buffer_t &surface, const ss_video_rect_t &clip);
    // Return whether output pixel x,y lies in a programmed blank interval, including wrap.
    bool blanked(int x, int y) const;
    // Resolve attribute palette entry index through VGA color selection and the Bt481.
    VideoColor attribute_color(uint8_t index) const;
public:
    // Borrow stored Oak OTI-087X register groups on the synchronized thread.
    // Inspection must not select an index, reset the attribute flip-flop or
    // consume RAMDAC data (Oak databook pp. 29-46; IBM VGA register definitions).
    // Share indexed register decoding with the port path; inspection must not
    // duplicate split-counter reconstruction (IBM VGA/XGA TRM pp. 2-64 to 2-95).
    uint8_t crtc_read(unsigned offset) const;
    uint8_t sequencer_read(uint8_t offset) const { return sequencer.data[offset]; }
    uint8_t gc_read(unsigned offset) const;
    uint8_t attribute_read(unsigned offset) const;
    // The optional backing view retains raw storage for snapshots; the port
    // view includes aliases and pin values without their I/O side effects.
    uint8_t extended_read(unsigned offset, bool backing = false) const;
    const auto &crtc_state() const { return crtc; }
    // Blanking at the output. The vertical start register holds the line where
    // blanking begins, minus 1 (IBM VGA/XGA TRM p. 2-73); blanking ends with
    // the line whose low eight bits equal the end register. Horizontal output
    // blanking also comes one character after its register counts. IBM's
    // 640 x 480 timing with these registers has 8 border dots and 8 border
    // lines at each side, and 18 characters and 29 lines of blanking (IBM PS/2
    // Video Subsystem, Oct 1990, Figs. 106, 108 and 109).
    unsigned horizontal_blank_first() const { return crtc.horz_blank_start + 1u; }
    unsigned horizontal_blank_length() const {
        unsigned const n = (crtc.horz_blank_end - crtc.horz_blank_start) & 63;
        return n ? n : 64;
    }
    unsigned vertical_blank_first() const { return crtc.vert_blank_start + 1u; }
    unsigned vertical_blank_length() const {
        unsigned const n = (crtc.vert_blank_end - crtc.vert_blank_start) & 255;
        return n ? n : 256;
    }
    const auto &sequencer_state() const { return sequencer; }
    uint8_t misc_output() const { return miscellaneous_output; }
    // Cable sense: true when a VGA monitor is attached.
    bool vga_monitor() const { return (monitor_override < 0 ? 1 : monitor_override) & 1; }
};
