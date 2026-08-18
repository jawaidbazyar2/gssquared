#pragma once

#include <cstdint>
#include <memory>

#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/frame/Frames.hpp"

/** Static full-page video decode modes (not cycle-accurate). */
enum class video_decode_mode_t {
    TEXT40 = 0,
    TEXT80,
    LORES40,
    LORES80,
    HIRES,
    HIRES_NOSHIFT,
    DHGR,
    SHR,
};

enum class video_render_mode_t {
    MONO = 0,
    NTSC,
    RGB,
};

/**
 * Static Apple II / IIgs page generator.
 * Decodes a contiguous page buffer directly to RGBA (no Frame560 bitstream).
 * Not for cycle-accurate artifacts — use VideoScanGenerator for the live display.
 * One instance per debug video view / dpp harness.
 */
class AppleII_View {
public:
    explicit AppleII_View(CharRom *char_rom);
    ~AppleII_View();
    AppleII_View(const AppleII_View &) = delete;
    AppleII_View &operator=(const AppleII_View &) = delete;

    void set_char_set(uint16_t char_set);
    void set_normal_alt(bool normal_alt);
    void set_flash_state(bool flash_state);
    void set_text_fg(uint8_t fg);
    void set_text_bg(uint8_t bg);

    /**
     * Decode a full page into RGBA.
     * SHR writes out640; other modes write out560.
     * `main`/`aux` are guest RAM pointers (no intermediate copy).
     * For SHR, main points at the 32K window starting at CPU $xx/2000.
     * If shr_phys_interleave is set, that window is Mega II aux physical
     * layout and bytes are translated with iigs_aux_linear_to_phys.
     */
    void generate(video_decode_mode_t decode, video_render_mode_t render,
                  const uint8_t *main, const uint8_t *aux,
                  Frame560RGBA *out560, Frame640 *out640,
                  bool shr_phys_interleave = false);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
