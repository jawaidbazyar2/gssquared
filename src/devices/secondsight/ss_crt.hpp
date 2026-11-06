/*
 * Turn an Oak OTI-087X active image into the complete monitor scan. Overscan
 * is the border color programmed in the attribute controller. Blanking is
 * the programmed interval where the video signal is suppressed.
 * SecondSightDisplay supplies the monitor window and completed card pixels.
 * OTI087::upload_frame reads the retained output and its changed-row range.
 * IBM VGA/XGA Technical Reference, ch. 2, CRTC blanking and Attribute $11;
 * Oak OTI-087 databook pp. 39-40 supplies the field and line-count extensions.
 * These retained host pixels never replace the card's native active raster.
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

// Owned by one SecondSightDisplay texture and used only on the render thread.
// The active card image remains separate from these retained monitor pixels.
class SecondSightCrt {
public:
    // CRTC (cathode-ray tube controller) counters wrap independently of the
    // monitor window. Horizontal units are pixels; vertical units are lines.
    // Each value belongs to this scan and is used only on the render thread.
    // IBM VGA/XGA TRM pp. 2-57/58, 2-73: blank end is a modulo comparator.
    struct Axis {
        // total is the counter period; start/count select the monitor window;
        // blank_start/blank_count select the signal-suppression interval.
        int total = 1, start = 0, count = 1, blank_start = 0, blank_count = 0;
        // A change to either window or blanking invalidates retained pixels.
        bool operator==(const Axis &other) const {
            return total == other.total && start == other.start && count == other.count &&
                blank_start == other.blank_start && blank_count == other.blank_count;
        }
        // Build the unblanked window from a positive counter period and the
        // first blank position/length. A full-period blank leaves no window.
        static Axis visible(int total, int first, int length) {
            length = std::min(total, length);
            return {total, (first + length) % total, total - length, first % total, length};
        }
        // Map a window offset to the counter, including negative window starts.
        int position(int offset) const { return ((start + offset) % total + total) % total; }
        // Is this nonnegative counter position inside the wrapping blank interval?
        bool blank(int position) const {
            return (position + total - blank_start) % total < blank_count;
        }
    };

    // Set the two monitor windows and output policy. enabled adds beam gaps
    // or separate fields; border is an opaque ARGB pixel; signal=false blacks
    // a scan the caller cannot present.
    // Line structure follows the signal, not the monitor-sense preference.
    // Interlace retains two fields; progressive scan has two output samples
    // per physical line. A CRTC-doubled 200-line mode already contains 400 lines.
    void configure(Axis horizontal, Axis vertical, bool enabled, bool interlaced, uint32_t border, bool signal = true,
                   int phase = 1, int phase_total = 2, bool odd_field = false, bool map_fields = false) {
        bool fields = enabled && interlaced;
        int repeat = enabled && !interlaced ? 2 : 1;
        const bool complex = interlaced && map_fields;
        const bool geometry_changed = !(x == horizontal) || !(y == vertical) || enabled_ != enabled ||
            fields_ != fields || interlaced_ != interlaced || signal_ != signal ||
            phase_ != phase || phase_total_ != phase_total || complex_ != complex;
        if (geometry_changed) { geometry_valid_ = false; chronology_dirty_ = false; }
        if (geometry_changed || border_ != border) valid = false;
        if (!geometry_changed && complex && !enabled && chronology_dependent_ && odd_field_ != odd_field)
            chronology_dirty_ = true;
        x = horizontal; y = vertical; enabled_ = enabled;
        fields_ = fields; interlaced_ = interlaced; repeat_ = repeat; border_ = border; signal_ = signal;
        phase_ = phase; phase_total_ = std::max(1, phase_total); complex_ = complex; odd_field_ = odd_field;
    }

    // Output texture dimensions in pixels, after progressive line repetition.
    int width() const { return x.count; }
    int height() const { return y.count * repeat_; }
    // True exposes two alternating field images instead of one complete image.
    bool fields() const { return fields_; }
    // Borrow the configured windows until the next configure call.
    const Axis &horizontal() const { return x; }
    const Axis &vertical() const { return y; }

    // The complete scan texture retains blanking and overscan. When no host
    // filter is active, later frames can update these native source rectangles
    // directly, without copying each row into another CPU image first.
    // Destination rectangle plus its source origin, in unrepeated scan units.
    // Regions belong to this scan; callers may borrow them until update().
    struct Region { int left, top, width, height, source_x, source_y; };
    const std::vector<Region> &active_regions() const { return regions; }
    // True permits direct source-to-texture copies for this source size; all
    // retained border/blank pixels must already have been uploaded.
    bool direct_upload(int width, int height) const {
        return valid && signal_ && !split_border && !enabled_ && !complex_ && width_ == width && height_ == height && damage_bottom < damage_top;
    }
    // Direct uploads leave the CPU staging image behind; a later filtered or
    // full upload must reconstruct it, including rows changed in earlier frames.
    void source_uploaded() { source_stale = true; }
    // Force a complete reconstruction, for example after texture recreation.
    void invalidate() { valid = false; }

    // Read a packed width-by-height array of opaque ARGB pixels. top/bottom
    // are inclusive changed source rows; bottom < top means no source changes.
    // Rebuild everything when geometry, border or signal changes; otherwise
    // retain rows outside the supplied damage range. Source storage is borrowed
    // only for this call, while output vectors remain owned by the scan.
    // Each entry is the border color before its exclusive beam-position end;
    // the final live color follows the last entry, just as in Oak raster replay.
    struct BorderChange { int end; uint32_t color; };
    void update(const uint32_t *source, int width, int height, int top, int bottom,
                const std::vector<BorderChange> *border_changes = nullptr) {
        const bool changing_border = border_changes && !border_changes->empty();
        if (changing_border || split_border) valid = false;
        split_border = changing_border;
        if (width_ != width || height_ != height) geometry_valid_ = false;
        if (source_stale || width_ != width || height_ != height) valid = false;
        // An unchanged native cache needs neither a scan walk nor an upload.
        if (valid && bottom < top && !chronology_dirty_) return;
        if (complex_) {
            update_fields(source, width, height, top, bottom, border_changes);
            return;
        }
        if (!valid) {
            // Resolve wrap, blank and active boundaries only when the mode or
            // color changes. Ordinary changed rows use copies/fills, not modulo
            // arithmetic or per-pixel conditionals in the WebAssembly loop.
            build_spans(width);
            // Coalesce consecutive source lines once per monitor geometry.
            // Combining these with the horizontal spans gives texture
            // rectangles even when the visible window wraps a CRTC counter.
            regions.clear();
            for (const auto &span : spans) {
                if (span.source < 0) continue;
                for (int row = 0; row < y.count;) {
                    const int sy = y.position(row);
                    if (!signal_ || y.blank(sy) || sy >= height) { ++row; continue; }
                    const int first = row++;
                    while (row < y.count && y.position(row) == sy + row - first &&
                        y.position(row) < height && !y.blank(y.position(row))) ++row;
                    regions.push_back({span.left, first, span.count, row - first, span.source, sy});
                }
            }
        }
        width_ = width; height_ = height;
        const size_t size = size_t(this->width()) * this->height();
        output[0].resize(size);
        if (fields_) output[1].resize(size);
        for (int row = 0; row < y.count; ++row) {
            const int sy = y.position(row);
            if (valid && (sy >= height || sy < top || sy > bottom)) continue;
            uint32_t *dest = output[0].data() + size_t(row * repeat_) * x.count;
            compose_line(dest, sy, source, width, height, changing_border ? border_changes : nullptr);
            if (fields_) {
                uint32_t *other = output[1].data() + size_t(row) * x.count;
                std::memcpy(other, dest, size_t(x.count) * 4);
                // Each field illuminates its own physical lines, overscan too.
                std::fill_n((sy & 1) ? dest : other, x.count, 0xff000000);
            } else if (enabled_) {
                uint32_t *gap = dest + x.count;
                // Integer quarter intensity makes the beam gaps visible after
                // ordinary window scaling. Packed masks keep channels separate.
                for (int col = 0; col < x.count; ++col)
                    gap[col] = 0xff000000 | ((dest[col] & 0x00fcfcfc) >> 2);
            }
            damage_top = std::min(damage_top, row * repeat_);
            damage_bottom = std::max(damage_bottom, (row + 1) * repeat_ - 1);
        }
        valid = true;
        source_stale = false;
    }

    // Borrow output pixels until the next update; field 1 requires fields().
    const uint32_t *pixels(int field = 0) const { return output[field].data(); }
    // Inclusive output rows needing upload; bottom < top means none.
    int top() const { return damage_top; }
    int bottom() const { return damage_bottom; }
    // Mark all retained output rows for a newly allocated texture.
    void full_upload() { damage_top = 0; damage_bottom = height() - 1; }
    // Clear damage only after the caller has uploaded the requested rows.
    void uploaded() { damage_top = height(); damage_bottom = -1; }

private:
    void build_spans(int width) {
        spans.clear();
        for (int col = 0; col < x.count; ++col) {
            const int sx = x.position(col);
            const int source = !signal_ || x.blank(sx) ? -2 : sx < width ? sx : -1;
            if (!spans.empty() && ((source < 0 && spans.back().source == source) ||
                (source >= 0 && spans.back().source >= 0 &&
                 spans.back().source + spans.back().count == source))) ++spans.back().count;
            else spans.push_back({col, source, 1});
        }
    }
    // Both the aligned and fractional-field paths use the same scan, blanking
    // and timed-border equations; only the destination row placement differs.
    void compose_line(uint32_t *dest, int sy, const uint32_t *source, int width, int height,
                      const std::vector<BorderChange> *changes) const {
        if (sy < 0 || !signal_ || y.blank(sy)) {
            std::fill_n(dest, x.count, 0xff000000);
            return;
        }
        for (const auto &span : spans) {
            if (span.source == -2) std::fill_n(dest + span.left, span.count, 0xff000000);
            else if (span.source >= 0 && sy < height)
                std::memcpy(dest + span.left, source + size_t(sy) * width + span.source, size_t(span.count) * 4);
            else if (!changes) std::fill_n(dest + span.left, span.count, border_);
            else {
                const int beam_y = interlaced_ ? (sy & 1) * (y.total / 2) + sy / 2 : sy;
                int done = 0;
                while (done < span.count) {
                    const int sx = x.position(span.left + done);
                    const int position = beam_y * x.total + sx;
                    const auto change = std::upper_bound(changes->begin(), changes->end(), position,
                        [](int p, const BorderChange &c) { return p < c.end; });
                    int part = std::min(span.count - done, x.total - sx);
                    if (change != changes->end()) part = std::min(part, change->end - position);
                    std::fill_n(dest + span.left + done, part, change == changes->end() ? border_ : change->color);
                    done += part;
                }
            }
        }
    }
    // For a fractional field phase, sample each line at one fixed horizontal
    // reference. This is a host reconstruction, not an analog beam model.
    // Odd line placement is relative to the aligned half-line convention;
    // source addresses and timed-border beam positions remain Oak coordinates.
    struct FieldRow { int even = -1, odd = -1, weight = 0, nearest_odd = -1; bool uniform = false; };
    void update_fields(const uint32_t *source, int width, int height, int top, int bottom,
                       const std::vector<BorderChange> *changes) {
        const int d = phase_total_;
        const int shift = -d - 2 * phase_;
        if (!geometry_valid_) {
            build_spans(width);
            field_rows.resize(y.count);
            chronology_dependent_ = false;
            for (int row = 0; row < y.count; ++row) {
                const int sy = y.position(row);
                auto &map = field_rows[row];
                map = {};
                if (!(sy & 1)) map.even = sy;
                // Find the odd source interval intersecting this output row,
                // retaining the fractional phase until coverage is sampled.
                int q = sy * d - shift;
                int k = q / d;
                if (q < 0 && q % d) --k;
                // In a gap, select the closest odd field line in the same
                // unwrapped coordinate; ties go to the earlier line.
                const int lower = (k & 1) ? k : k - 1;
                const int nearest = q - lower * d <= (lower + 2) * d - q ? lower : lower + 2;
                map.nearest_odd = ((nearest % y.total) + y.total) % y.total;
                if (!(k & 1)) ++k;
                const int overlap = std::max(0, std::min((sy + 1) * d, (k + 1) * d + shift) -
                                                  std::max(sy * d, k * d + shift));
                if (overlap) {
                    map.odd = ((k % y.total) + y.total) % y.total;
                    map.weight = overlap;
                }
                // Outside active video, two equally blank or equally overscan
                // lines cannot change when field chronology changes. Timed
                // border colors are handled conservatively at composition.
                if (map.even >= 0 && map.odd >= 0) {
                    auto uniform = [&](int line) {
                        return !signal_ || y.blank(line) ? 1 : line >= height ? 2 : 0;
                    };
                    const int a = uniform(map.even), b = uniform(map.odd);
                    map.uniform = a && a == b;
                }
                if (!enabled_ && map.even >= 0 && map.odd >= 0) chronology_dependent_ = true;
                // Cache geometry independently of which field is current.
                if (map.even < 0 && map.odd < 0 && !enabled_) {
                    chronology_dependent_ = true;
                }
            }
            scratch[0].resize(x.count);
            scratch[1].resize(x.count);
            geometry_valid_ = true;
        }
        width_ = width; height_ = height;
        output[0].resize(size_t(x.count) * y.count);
        if (fields_) output[1].resize(size_t(x.count) * y.count);
        for (int row = 0; row < y.count; ++row) {
            auto map = field_rows[row];
            const bool chronology_row = !enabled_ &&
                ((map.even >= 0 && map.odd >= 0 && !(map.uniform && !changes)) ||
                 (map.even < 0 && map.odd < 0));
            if (map.even < 0 && map.odd < 0 && !enabled_) {
                // Reconstruct uncovered rows without adding artificial black.
                const int sy = y.position(row);
                if (odd_field_) { map.odd = map.nearest_odd; map.weight = d; }
                else map.even = (sy + y.total - 1) % y.total;
            }
            if (valid && !(chronology_dirty_ && chronology_row) &&
                (map.even < top || map.even > bottom) &&
                (map.odd < top || map.odd > bottom)) continue;
            uint32_t *out = output[0].data() + size_t(row) * x.count;
            uint32_t *other = fields_ ? output[1].data() + size_t(row) * x.count : nullptr;
            if (fields_) {
                compose_line(out, map.even, source, width, height, changes);
                compose_line(other, map.odd, source, width, height, changes);
                if (map.weight > 0 && map.weight < d) for (int col = 0; col < x.count; ++col) {
                    const uint32_t odd = other[col];
                    auto covered = [&](int bit) { return (((odd >> bit) & 255) * map.weight / d) << bit; };
                    other[col] = 0xff000000 | covered(16) | covered(8) | covered(0);
                }
            } else if (map.odd < 0 || (map.even >= 0 && !odd_field_)) {
                compose_line(out, map.even, source, width, height, changes);
            } else if (map.even < 0 || map.weight == d) {
                compose_line(out, map.odd, source, width, height, changes);
            } else {
                compose_line(scratch[0].data(), map.even, source, width, height, changes);
                compose_line(scratch[1].data(), map.odd, source, width, height, changes);
                // Only overlapping partial lines need per-channel coverage.
                for (int col = 0; col < x.count; ++col) {
                    const uint32_t a = scratch[1][col], b = scratch[0][col];
                    auto channel = [&](int bit) {
                        return ((((a >> bit) & 255) * map.weight + ((b >> bit) & 255) *
                            (d - map.weight) + d / 2) / d) << bit;
                    };
                    out[col] = 0xff000000 | channel(16) | channel(8) | channel(0);
                }
            }
            damage_top = std::min(damage_top, row);
            damage_bottom = std::max(damage_bottom, row);
        }
        valid = true; source_stale = false; chronology_dirty_ = false;
    }
    Axis x, y;
    int width_ = 0, height_ = 0, repeat_ = 1;
    bool enabled_ = false, fields_ = false, interlaced_ = false, valid = false, signal_ = true, source_stale = false;
    bool split_border = false;
    bool complex_ = false, odd_field_ = false, chronology_dependent_ = false, chronology_dirty_ = false;
    bool geometry_valid_ = false;
    int phase_ = 1, phase_total_ = 2;
    uint32_t border_ = 0xff000000;
    int damage_top = 0, damage_bottom = -1;
    // One horizontal run: source >= 0 indexes active pixels, -1 selects
    // overscan and -2 selects black. Used only while rebuilding this scan.
    struct Span { int left, source, count; };
    std::vector<Span> spans;
    std::vector<Region> regions;
    std::vector<FieldRow> field_rows;
    std::vector<uint32_t> scratch[2];
    std::vector<uint32_t> output[2];
};
