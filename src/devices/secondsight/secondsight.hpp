#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include "util/DebugFormatter.hpp"
#include "gs2.hpp"
#include "videosystem.hpp"
#include "NClock.hpp"
#include "vga_render.hpp"
#include "vga_render_text_9x16.hpp"
#include "vga_render_text_9x16_present.hpp"
#include "vga_mode_tables.hpp"
#include "paths.hpp"

struct computer_t;


#define SECOND_SIGHT_FB_SIZE 1024 * 1024
/** Z180 SRAM ($00/0000–$01/FFFF per Second Sight memory map). */
#define SS_Z180_SRAM_SIZE (128 * 1024)
/** User-uploaded 8x16 font lives here (SetTextFont $03 / ROM copy_font). */
#define SS_Z180_USER_FONT_ADDR 0xF000

class SecondSight {
    // "RAW" mode rec for setting user modes.
    struct vga_mode_rec {
        unsigned char misc;
        unsigned char clockMode;
        unsigned char featureCtl;
    
        unsigned char seq02, seq03, seq04;
    
        unsigned char crt_regs[0x19];
        unsigned char graph_regs[0x09];
        unsigned char attr_regs[0x15];
    
        unsigned char ext_regs[20]; /* NOT linearly mapped - see examples */
        unsigned char unused[30];
    } vga_mode_rec;

    uint8_t *frame_buffer;
    uint8_t *z180_sram = nullptr;

    uint8_t reg_cmd = 0;

    // 0 = handshake off - set when data transfer is complete
    // 1 = handshake on - set when command is received and ready
    // A5 = cmd completed no error - set when long-running command is completed successfully
    // A6 = cmd error - set when long-running command is completed with error
    uint8_t reg_handshake = 0;

    uint8_t cmd_buffer[256] = {0};
    
    //uint16_t data_index = 0;
    //uint16_t data_length = 0;
    uint8_t resp_buffer[256] = {0};
    uint16_t resp_length = 0;

    int active_command = -1; // no command is active
    int command_step = 0; // 0 = none; others are command specific, but increment as each step completes.

    uint8_t vga_mode_num = 0;
    uint16_t vga_active = 0; // 1 means VGA mode active ("do not emulate current Apple II mode")
    uint16_t display_enabled = 1; // 1 means display is enabled

    union {
        uint8_t user_mode_data[84] = {0};
        struct vga_mode_rec user_mode_rec;
    };

    uint16_t res_x = 640;
    uint16_t res_y = 480;

    /* static constexpr uint8_t cmd_lengths[] = {
        0x01, // GetStatus
        0x03, // SetMode
        0x0B, // Upload Code / Data
        // Upload Bitmap (not implemented)
        0x0A, // Scroll Screen
        0x01, // Screen Off
        0x01, // Screen On
        0x04, // Set Palette
        0x05, // SetPaletteEntry
        0x02, // SetBorder
        0x06, // Run Code
        0x08, // Clear Screen
        0x02, // SetShadow
        0x07, // SetVGAReg
        0x06, // GetVGAReg
        85, // SetUserMode (then 84 bytes of mode selection data),
        0x04, // SetTextFont (unimplmented as of ROM 01)

    }; */

    video_system_t *vs;
    MMU *mmu;
    NClock *clock;
    SDL_Texture *tex_16bpp = nullptr;
    SDL_Texture *tex_24bpp = nullptr;
    SDL_Texture *tex_text = nullptr;

    SDL_Color vga_palette[256] = {};
    uint8_t palette_rgb[256][3] = {};
    uint8_t *rgb24_buffer = nullptr;

    uint8_t crt_start_addr_high = 0; // CRTC reg 0x0C
    uint8_t crt_start_addr_low = 0;  // CRTC reg 0x0D
    uint8_t crt_hdisplay_end = 0;  // CRTC reg 0x01 (horizontal display end)
    uint8_t crt_offset = 0;        // CRTC reg 0x13 (scanline offset, in words)
    uint8_t crt_char_width = 8;    // pixels per CRTC character clock
    uint32_t screen_base_addr = 0;
    uint16_t fb_pitch = 0;
    /** SetTextFont index ($00–$03); $FF = never set. */
    uint8_t text_font_index = 0xFF;

    struct upload_log_entry_t {
        uint8_t code_data_flag = 0;
        uint32_t dest = 0;
        uint32_t length = 0;
        /** Host-side read address (65816 reads here; bytes stream via C0B1 only). */
        uint32_t host_src = 0;
    };
    static constexpr int SS_UPLOAD_LOG_MAX = 8;
    /** Only print bulk uploads to stdout (avoids 2-byte spam). */
    static constexpr uint32_t SS_UPLOAD_LOG_PRINTF_MIN = 64;
    upload_log_entry_t upload_log[SS_UPLOAD_LOG_MAX] = {};
    int upload_log_total = 0;
    int upload_log_next = 0;
    uint32_t upload_small_total = 0;

    static const char *text_font_label(uint8_t index) {
        switch (index) {
            case 0: return "ROM standard ($00)";
            case 1: return "ROM alternate ($01)";
            case 2: return "PC ANSI ($02)";
            case 3: return "User @ Z180 $F000 ($03)";
            default: return "unknown";
        }
    }

    void record_upload_log(uint8_t flag, uint32_t dest, uint32_t length, uint32_t host_src) {
        if (length < SS_UPLOAD_LOG_PRINTF_MIN) {
            upload_small_total++;
            return;
        }
        upload_log[upload_log_next % SS_UPLOAD_LOG_MAX] = {flag, dest, length, host_src};
        upload_log_next++;
        upload_log_total++;
        printf("SecondSight upload: VRAM dest=%06X len=%u host_src=%06X (flag=%d)\n",
            dest, length, host_src, flag);
    }

    void apply_text_font(uint8_t font_index) {
        text_font_index = font_index;
        printf("SecondSight SetTextFont: %02X (%s)\n", font_index, text_font_label(font_index));
        switch (font_index) {
            case 3:
                if (z180_sram != nullptr) {
                    vga_text_9x16_load_font_from_vram(z180_sram + SS_Z180_USER_FONT_ADDR,
                        SS_VRAM_FONT_GLYPH_BYTES);
                }
                break;
            case 0:
            case 1:
            case 2:
            default:
                break;
        }
    }

    // Various apple II code is:
    // edge-detect on the handshake value;
    // and, takes some time to get around to the point where it can read the handshake value.
    // So if these values are too low, the A2 will not see the correct edge and will hang.
    constexpr static uint32_t WAIT_CYCLES = 60;
    // this value is used for long-running commands that would get executed during the SS event
    // loop in the rom, and might take a long time (1000s of cycles) to execute. For example, clearscreen and scrollscreen.
    // Here we accelerate them insanely.
    constexpr static uint32_t LONGRUN_WAIT_CYCLES = 60;

    uint32_t crtc_char_clocks_per_line() const {
        uint32_t char_clocks = (uint32_t)crt_hdisplay_end + 1;
        if (char_clocks <= 1 && crt_char_width > 0 && current_vga_mode.width >= crt_char_width) {
            char_clocks = current_vga_mode.width / crt_char_width;
        }
        return char_clocks > 0 ? char_clocks : 1;
    }

    inline bool is_text_mode() const {
        return current_vga_mode.graphics == TG_TEXT;
    }

    // SetUserMode DMA payload (misc through ext_regs); excludes trailing padding.
    static constexpr size_t SS_VGA_MODE_REC_DMA_BYTES = 84;

    void load_vga_mode_rec(const ss_vga_mode_rec *src) {
        memcpy(&user_mode_rec, src, SS_VGA_MODE_REC_DMA_BYTES);
    }

    /** Apply SecondSight ROM modetable values to emulated CRTC/FB state. */
    void apply_rom_vga_mode(const ss_vga_mode_rec *rom, uint8_t mode_num) {
        load_vga_mode_rec(rom);

        crt_hdisplay_end = rom->crt_regs[0x01];
        crt_offset = rom->crt_regs[0x13];
        crt_start_addr_high = rom->crt_regs[0x0C];
        crt_start_addr_low = rom->crt_regs[0x0D];

        for (int i = 0; i < sizeof(vga_modes) / sizeof(vga_mode_t); i++) {
            if (vga_modes[i].mode == mode_num) {
                current_vga_mode = vga_modes[i];
                break;
            }
        }
        vga_mode_num = mode_num;

        if (mode_num == 0x03) {
            crt_char_width = 9;
            fb_pitch = (uint16_t)vga_text_pitch_from_crtc_offset(crt_offset);
            res_x = VGA_TEXT_SCREEN_W;
            res_y = VGA_TEXT_SCREEN_H;
        } else {
            mode_info info;
            analyze_vga_mode(&user_mode_rec, &info);
            apply_analyzed_mode(&user_mode_rec, &info);
            vga_mode_num = mode_num;
        }
        update_screen_base_addr();
        // OTI extended regs not yet tracked; for mode 03h the text buffer
        // always lives at VRAM offset 0x01'0000 (OTI reg 0x22 / ext_regs[12] = 0x08).
        if (mode_num == 0x03) {
            screen_base_addr = 0x010000;
        }
    }

    uint32_t crtc_bytes_per_address_unit() const {
        uint32_t char_clocks = crtc_char_clocks_per_line();
        if (current_vga_mode.graphics == TG_TEXT && char_clocks < current_vga_mode.width) {
            // Trust the known column count when CRTC reg 0x01 is not programmed yet.
            char_clocks = current_vga_mode.width;
        }
        if (fb_pitch > 0 && char_clocks > 0) {
            return fb_pitch / char_clocks;
        }
        // No pitch yet — derive from depth and character width.
        uint32_t depth = current_vga_mode.color_depth;
        if (depth == 0) {
            depth = 8;
        }
        return (crt_char_width * depth + 7) / 8;
    }

    void update_screen_base_addr() {
        // CRTC start address (regs 0x0C/0x0D) counts in character-clock units.
        // Each unit covers (pitch / char_clocks_per_line) bytes in the linear FB.
        uint32_t crtc_start = ((uint32_t)crt_start_addr_high << 8) | crt_start_addr_low;
        screen_base_addr = crtc_start * crtc_bytes_per_address_unit();
    }

    void sync_crtc_timing_defaults() {
        if (crt_char_width == 0) {
            crt_char_width = 8;
        }
        if (current_vga_mode.graphics == TG_TEXT) {
            if (crt_hdisplay_end < 0x10) {
                crt_hdisplay_end = (uint8_t)(current_vga_mode.width - 1);
            }
            if (crt_offset == 0) {
                crt_offset = (uint8_t)(current_vga_mode.width / 2);
            }
            fb_pitch = (uint16_t)vga_text_pitch_from_crtc_offset(crt_offset);
        } else if (current_vga_mode.width >= crt_char_width) {
            crt_hdisplay_end = (uint8_t)((current_vga_mode.width / crt_char_width) - 1);
        }
    }

    void write_crtc_reg(uint8_t index, uint8_t value) {
        switch (index) {
            case 0x01:
                crt_hdisplay_end = value;
                update_screen_base_addr();
                break;
            case 0x0C:
                crt_start_addr_high = value;
                update_screen_base_addr();
                break;
            case 0x0D:
                crt_start_addr_low = value;
                update_screen_base_addr();
                break;
            case 0x13:
                crt_offset = value;
                // Padded/virtual width: memory pitch may exceed visible width.
                if (crt_offset > 0) {
                    uint32_t offset_pitch = is_text_mode()
                        ? (uint32_t)vga_text_pitch_from_crtc_offset(crt_offset)
                        : (uint32_t)crt_offset * 2;
                    if (offset_pitch > fb_pitch) {
                        fb_pitch = (uint16_t)offset_pitch;
                        update_screen_base_addr();
                    }
                }
                break;
        }
    }

    uint8_t read_crtc_reg(uint8_t index, uint16_t address) {
        switch (index) {
            case 0x01:
                return crt_hdisplay_end;
            case 0x0C:
                return crt_start_addr_high;
            case 0x0D:
                return crt_start_addr_low;
            case 0x13:
                return crt_offset;
        }
        return 0;
    }

    void init_default_palette() {
        for (int i = 0; i < 256; i++) {
            vga_palette[i].r = (uint8_t)i;
            vga_palette[i].g = (uint8_t)i;
            vga_palette[i].b = (uint8_t)i;
            vga_palette[i].a = 255;
        }
        rebuild_palette_rgb();
    }

    void rebuild_palette_rgb() {
        for (int i = 0; i < 256; i++) {
            palette_rgb[i][0] = vga_palette[i].r;
            palette_rgb[i][1] = vga_palette[i].g;
            palette_rgb[i][2] = vga_palette[i].b;
        }
    }

    void sync_vga_palette_from_rgb() {
        for (int i = 0; i < 256; i++) {
            vga_palette[i].r = palette_rgb[i][0];
            vga_palette[i].g = palette_rgb[i][1];
            vga_palette[i].b = palette_rgb[i][2];
            vga_palette[i].a = 255;
        }
    }

    void set_palette_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
        vga_palette[index].r = r;
        vga_palette[index].g = g;
        vga_palette[index].b = b;
        vga_palette[index].a = 255;
        palette_rgb[index][0] = r;
        palette_rgb[index][1] = g;
        palette_rgb[index][2] = b;
    }

    public:
        SecondSight(video_system_t *video_system, MMU *mmu, NClock *clock) : vs(video_system), mmu(mmu), clock(clock) {
            frame_buffer = new uint8_t[SECOND_SIGHT_FB_SIZE];
            memset(frame_buffer, 0, SECOND_SIGHT_FB_SIZE);
            z180_sram = new uint8_t[SS_Z180_SRAM_SIZE];
            memset(z180_sram, 0, SS_Z180_SRAM_SIZE);

            rgb24_buffer = new uint8_t[SS_RGB24_BUFFER_SIZE];
            init_default_palette();

            tex_16bpp = SDL_CreateTexture(vs->renderer, SDL_PIXELFORMAT_XRGB1555, SDL_TEXTUREACCESS_TARGET, 800, 600);
            if (!tex_16bpp) {
                printf("SecondSight: failed to create 16bpp texture\n");
            }
            tex_24bpp = SDL_CreateTexture(vs->renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, SS_MAX_WIDTH, SS_MAX_HEIGHT);
            if (!tex_24bpp) {
                printf("SecondSight: failed to create 24bpp texture\n");
            }
            tex_text = SDL_CreateTexture(vs->renderer, SDL_PIXELFORMAT_ARGB8888,
                SDL_TEXTUREACCESS_STREAMING, VGA_TEXT_SCREEN_W, VGA_TEXT_SCREEN_H);
            if (!tex_text) {
                printf("SecondSight: failed to create text texture\n");
            } else {
                SDL_SetTextureScaleMode(tex_text, SDL_SCALEMODE_NEAREST);
            }
            display_enabled = true;
        }
        ~SecondSight() {
            delete[] frame_buffer;
            delete[] z180_sram;
            delete[] rgb24_buffer;
            SDL_DestroyTexture(tex_16bpp);
            SDL_DestroyTexture(tex_24bpp);
            SDL_DestroyTexture(tex_text);
        }

        void reset() {
            dma_address = nullptr;
            dma_length = 0;
            display_enabled = 1;

            cmd_buffer[0] = 0;
            resp_length = 0;
            resp_buffer[0] = 0;
            reg_handshake = 0;
            vga_active = 0;
            vga_mode_num = 0;
            res_x = 640;
            res_y = 480;
            current_vga_mode = {0};
            active_command = -1;
            crt_start_addr_high = 0;
            crt_start_addr_low = 0;
            crt_hdisplay_end = 0;
            crt_offset = 0;
            crt_char_width = 8;
            screen_base_addr = 0;
            text_font_index = 0xFF;
            upload_log_total = 0;
            upload_log_next = 0;
            upload_small_total = 0;
            fb_pitch = 0;
        }

        uint8_t *dma_address = nullptr;
        uint32_t dma_length = 0;
        
        enum tg_t {
            TG_TEXT = 0,
            TG_GRAPHICS = 1,
        };

        struct vga_mode_t {
            uint8_t mode;
            bool vgamode;
            tg_t graphics;
            uint16_t width;
            uint16_t height;
            uint8_t color_depth; // in bits
            uint8_t bitspercolor;
        };

        static constexpr vga_mode_t vga_modes[] = {
            {0x01, true, TG_TEXT, 40, 25, 4, 4},
            {0x03, true, TG_TEXT, 80, 25, 4, 4},
            {0x13, true, TG_GRAPHICS, 320, 200, 8, 6},
            {0x53, true, TG_GRAPHICS, 640, 480, 8, 8},
            {0x5C, true, TG_GRAPHICS, 640, 480, 16, 5},
            {0x5F, true, TG_GRAPHICS, 640, 480, 24, 8},
            // there is no 0x60 mode. must be used as a fake mode number in the library.
            {0x61, true, TG_GRAPHICS, 640, 400, 8, 8},

            // "emulated" modes - these were internal for SS and just here for documentation.
            {0xFA, false, TG_GRAPHICS, 560, 192, 8, 4},
            {0xFB, false, TG_GRAPHICS, 280, 192, 8, 4},
            {0xFC, false, TG_TEXT, 40, 24, 4, 4},
            {0xFD, false, TG_TEXT,  80, 24, 4, 4},
            {0xFE, false, TG_GRAPHICS, 640, 400, 8, 4},
        };

        vga_mode_t current_vga_mode = {0};

        enum dma_direction_t {
            DMA_DIRECTION_IN = 0,
            DMA_DIRECTION_OUT = 1,
        };
        dma_direction_t dma_direction = DMA_DIRECTION_IN;

        void setup_dma(dma_direction_t direction, uint8_t *address, uint32_t length) {
            dma_direction = direction;
            dma_address = address;
            dma_length = length;
            reg_handshake = 0x01;
        }

    typedef struct {
        int hres;          // Horizontal resolution in pixels
        int vres;          // Vertical resolution in lines
        int depth;         // Bits per pixel (4, 8, 15, 16, 24)
        int pitch;         // Bytes per scanline in the frame buffer
        int char_width;    // Character width (8 or 9 pixels)
        int double_scan;   // Line doubling enabled
    } mode_info;

    int analyze_vga_mode(struct vga_mode_rec *mode, mode_info *info) {
        int hdisplay, vdisplay, overflow, max_scanline;
        int shift_mode, offset;
        int bytes_per_scanline;
        float ratio;
        
        hdisplay = mode->crt_regs[0x01];
        offset = mode->crt_regs[0x13];
        shift_mode = (mode->graph_regs[0x05] >> 5) & 0x03;
        int graphics_mode = (mode->graph_regs[0x06] & 0x01) ? 1 : 0;
        
        ratio = (offset > 0) ? (float)(hdisplay + 1) / offset : 1.0f;
        
        if (!graphics_mode) {
            info->depth = 4;
        } else if (shift_mode == 0) {
            info->depth = 4;
        } else if (shift_mode == 1) {
            info->depth = 8;
        } else if (shift_mode == 2) {
            if (ratio > 2.5) {
                info->depth = 24;
            } else if (ratio > 1.5) {
                info->depth = 16;
            } else {
                info->depth = 8;
            }
        } else {
            info->depth = 8;
        }
        
        bytes_per_scanline = (hdisplay + 1) * 8;

        switch (info->depth) {
            case 4:
                info->hres = bytes_per_scanline * 2;
                break;
            case 8:
                info->hres = bytes_per_scanline;
                break;
            case 16:
                info->hres = bytes_per_scanline / 2;
                break;
            case 24:
                info->hres = bytes_per_scanline / 3;
                break;
            default:
                info->hres = bytes_per_scanline;
        }

        vdisplay = mode->crt_regs[0x12];
        overflow = mode->crt_regs[0x07];

        if (overflow & 0x02) vdisplay |= 0x100;
        if (overflow & 0x40) vdisplay |= 0x200;

        info->vres = vdisplay + 1;

        max_scanline = mode->crt_regs[0x09];
        info->double_scan = (max_scanline & 0x80) ? 1 : 0;
        if (info->double_scan) {
            info->vres /= 2;
        }

        info->char_width = 8;
        info->pitch = bytes_per_scanline;

        return 0;
    }

    void apply_analyzed_mode(struct vga_mode_rec *mode, const mode_info *info) {
        current_vga_mode.mode = 0xFF;
        current_vga_mode.vgamode = true;
        current_vga_mode.graphics = (info->depth <= 4) ? TG_TEXT : TG_GRAPHICS;
        current_vga_mode.width = info->hres;
        current_vga_mode.height = info->vres;
        current_vga_mode.color_depth = info->depth;
        if (info->depth == 16) {
            current_vga_mode.bitspercolor = 5;
        } else if (info->depth == 8) {
            current_vga_mode.bitspercolor = 8;
        } else {
            current_vga_mode.bitspercolor = 8;
        }
        fb_pitch = info->pitch;
        res_x = info->hres;
        res_y = info->vres;
        vga_active = 1;
        vga_mode_num = 0xFF;
        crt_hdisplay_end = mode->crt_regs[0x01];
        crt_offset = mode->crt_regs[0x13];
        crt_char_width = (uint8_t)info->char_width;
        crt_start_addr_high = mode->crt_regs[0x0C];
        crt_start_addr_low = mode->crt_regs[0x0D];
        update_screen_base_addr();
    }

    void print_mode_info(struct vga_mode_rec *mode) {
        mode_info info;
        int i;
        
        printf("Raw VGA Register Data:\n");
        printf("Misc Output:    0x%02X\n", mode->misc);
        printf("Clock Mode:     0x%02X\n", mode->clockMode);
        printf("Feature Ctrl:   0x%02X\n", mode->featureCtl);
        printf("Sequencer 02:   0x%02X\n", mode->seq02);
        printf("Sequencer 03:   0x%02X\n", mode->seq03);
        printf("Sequencer 04:   0x%02X\n", mode->seq04);
        
        printf("\nCRT Registers:\n");
        for (i = 0; i < 0x19; i++) {
            printf("  CRT[0x%02X] = 0x%02X", i, mode->crt_regs[i]);
            if (i == 0x01) printf("  (H Display End)");
            if (i == 0x07) printf("  (Overflow)");
            if (i == 0x09) printf("  (Max Scanline)");
            if (i == 0x12) printf("  (V Display End)");
            printf("\n");
        }
        
        printf("\nGraphics Registers:\n");
        for (i = 0; i < 0x09; i++) {
            printf("  GFX[0x%02X] = 0x%02X\n", i, mode->graph_regs[i]);
        }
        
        printf("\nAttribute Registers:\n");
        for (i = 0; i < 0x15; i++) {
            printf("  ATR[0x%02X] = 0x%02X\n", i, mode->attr_regs[i]);
        }
        
        // Analyze the mode
        analyze_vga_mode(mode, &info);
        
        printf("\n========== ANALYZED MODE ==========\n");
        printf("Resolution:     %d x %d\n", info.hres, info.vres);
        printf("Color Depth:    %d bits per pixel\n", info.depth);
        printf("Pitch:          %d bytes/line\n", info.pitch);
        printf("Char Width:     %d pixels\n", info.char_width);
        printf("Line Doubling:  %s\n", info.double_scan ? "Yes" : "No");
        
        // Guess the mode name
        if (info.hres == 640 && info.vres == 480 && info.depth == 8) {
            printf("Mode:           640x480x256 (Mode 0x53)\n");
        } else if (info.hres == 640 && info.vres == 480 && info.depth == 4) {
            printf("Mode:           640x480x16 (VGA)\n");
        } else if (info.hres == 640 && info.vres == 400 && info.depth == 8) {
            printf("Mode:           640x400x256 (Mode 0x61)\n");
        } else if (info.hres == 320 && info.vres == 200 && info.depth == 8) {
            printf("Mode:           320x200x256 (Mode 0x13)\n");
        } else if (info.hres == 800 && info.vres == 600) {
            printf("Mode:           800x600 (SVGA)\n");
        } else {
            printf("Mode:           Custom/Unknown\n");
        }
    }

    // Test with your example data
    /* void test_mode() {
        unsigned char example[] = {
            0xEF, 0x01, 0xA0, 0x0F, 0x00, 0x0E, // misc, clock, feature, seq02-04
            0xC3, 0x9F, 0xA1, 0x85, 0xA7, 0x1F, 0x0B, 0x3E, // CRT 0x00-0x07
            0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // CRT 0x08-0x0F
            0xEA, 0x8C, 0xDF, 0x50, 0x00, 0xE7, 0x04, 0xC3, // CRT 0x10-0x17
            0xFF,                                             // CRT 0x18
            0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF, // Graphics
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // Attribute 0-7
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, // Attribute 8-15
            0x01, 0x00, 0x0F, 0x00, 0x00,                   // Attribute 16-20
            0x05, 0x10, 0x07, 0x00, 0x0F, 0xC8, 0x00, 0x00, // Extended
            0x00, 0x0F, 0x05, 0x04, 0x0C, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };
        
        vga_mode_rec *mode = (vga_mode_rec *)example;
        print_mode_info(mode);
    } */

        // a DMA completing will trigger a command_step increment.
        // 1: EXECUTE
        // 2: DATA_OUT
        // 3: DONE
        void cmd_get_status() {
            if (command_step == 1) {
                resp_buffer[0] = 0x00; // ??? what is this??
                resp_buffer[1] = 'G';
                resp_buffer[2] = 'S';
                resp_buffer[3] = 'V';
                resp_buffer[4] = 'G';
                resp_buffer[5] = 'A';
                resp_buffer[6] = 0x05; // size of following record data
                resp_buffer[7] = 0x14; // version 1.4
                resp_buffer[8] = vga_active; 
                resp_buffer[9] = vga_mode_num;
                resp_buffer[0x0A] = 1; // 1MB video RAM
                resp_buffer[0x0B] = 0x10; // VGA monitor (0x00 = AppleColor)

                setup_dma(DMA_DIRECTION_OUT, resp_buffer, 0x0C);
            } else if (command_step == 2) {
                // DMA complete, we're done.
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        // a DMA completing will trigger a command_step increment.
        // 1: DATA_IN
        // 2: EXECUTE
        // 3: DONE
        void cmd_set_mode() {
            // $01: $XX: Screen Mode to switch to
            //   $01 - 40x25
            //   $02 - 80x25
            //   $53 - 640x480x256 (VGA)
            //   $13 - 320x200x256
            //   $61 - 640x400x256
            // $02: $EE - emulation flag
            //   $01: VGA mode
            //   $00: emulation mode
            // look for mode, if we don't find it then error.

            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 0x02);
            } else if (command_step == 2) {
                if (cmd_buffer[1] == 0xFF) {
                    mode_info info;
                    analyze_vga_mode(&user_mode_rec, &info);
                    apply_analyzed_mode(&user_mode_rec, &info);
                    vga_active = cmd_buffer[2] == 0x00 ? 0 : 1;
                } else {
                    if (cmd_buffer[1] == 0x03) {
                        apply_rom_vga_mode(&SS_ROM_VGA_TEXT_80X25, 0x03);
                    } else {
                        for (int i = 0; i < sizeof(vga_modes) / sizeof(vga_mode_t); i++) {
                            if (vga_modes[i].mode == cmd_buffer[1]) {
                                current_vga_mode = vga_modes[i];
                                vga_mode_num = cmd_buffer[1];
                                res_x = current_vga_mode.width;
                                res_y = current_vga_mode.height;
                                fb_pitch = current_vga_mode.width
                                    * (current_vga_mode.color_depth / 8);
                                crt_char_width = 8;
                                sync_crtc_timing_defaults();
                                update_screen_base_addr();
                                break;
                            }
                        }
                    }
                    // silently ignores invalid mode number, but respect this.
                    vga_active = cmd_buffer[2] == 0x00 ? 0 : 1;
                }
                longrun_cycle_target = clock->get_cycles() + LONGRUN_WAIT_CYCLES; // set up trigger
                pending_status = 0xA5;
            }
            display_enabled = 1;
        }

        void cmd_set_user_mode() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, user_mode_data, 84);
            } else if (command_step == 2) {
                // DMA complete
                printf("SecondSight: cmd_set_user_mode\n");
                for (int i = 0; i < 84; i++) {
                    printf("%02X ", user_mode_data[i]);
                }
                printf("\n");

                mode_info info;
                analyze_vga_mode(&user_mode_rec, &info);
                apply_analyzed_mode(&user_mode_rec, &info);
                print_mode_info(&user_mode_rec);
                printf("SecondSight: applied user mode, rendering %dx%d %dbpp pitch=%d base=%X\n",
                    current_vga_mode.width, current_vga_mode.height,
                    current_vga_mode.color_depth, fb_pitch, screen_base_addr);

                command_step = 0;
                reg_handshake = 0x00;
                display_enabled = 1;
            }
        }

        void cmd_clear_screen() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 7);
            } else if (command_step == 2) {
                // DMA complete
                uint8_t color = cmd_buffer[1];
                uint32_t address = (cmd_buffer[4] << 16) | (cmd_buffer[3] << 8) | cmd_buffer[2];
                uint32_t length = (cmd_buffer[7] << 16) | (cmd_buffer[6] << 8) | cmd_buffer[5];
                memset(frame_buffer+address, color, length);
                command_step = 0;
                reg_handshake = 0x00;
                longrun_cycle_target = clock->get_cycles() + LONGRUN_WAIT_CYCLES; // set up trigger
                pending_status = 0xA5;
            }
        }

        void cmd_set_text_font() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 1);
            } else if (command_step == 2) {
                apply_text_font(cmd_buffer[1]);
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        /*
        1: DMA_IN - get arguments
        2: EXECUTE
        3: DMA_IN - get data to load!
        4: DONE
        */

        uint32_t dma_offset = 0;
        uint32_t dma_length_remaining = 0;
        uint32_t dma_this_chunk = 0;
        uint32_t upload_start_offset = 0;
        uint32_t upload_total_length = 0;
        uint8_t upload_code_data_flag = 0;
        uint32_t upload_host_src = 0;

        void cmd_upload_code_data() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 10);
            } else if (command_step == 2) {
                // args DMA complete..
                // let handshake be 0 for a bit.
                longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                pending_status = 0x00;
            } else if (command_step == 3) {
                upload_code_data_flag = cmd_buffer[1];
                dma_offset = (cmd_buffer[4] << 16) | (cmd_buffer[3] << 8) | cmd_buffer[2];
                uint32_t length = (cmd_buffer[7] << 16) | (cmd_buffer[6] << 8) | cmd_buffer[5];
                upload_host_src = ((uint32_t)cmd_buffer[10] << 16)
                    | ((uint32_t)cmd_buffer[9] << 8) | (uint32_t)cmd_buffer[8];
                upload_start_offset = dma_offset;
                upload_total_length = length;
                if (length == 0) { // handle special case of zero length
                    command_step = 0;
                    reg_handshake = 0x00;
                    return;
                }
                dma_length_remaining = length;
                dma_this_chunk = (dma_length_remaining & 0xFF0000) ? 0x1'0000 : dma_length_remaining;
                // ROM firmware bug: upload_chunk always adds #8 to the dest bank, so
                // flag=0 ("code") still lands in VGA VRAM (bank $08+), not Z180 SRAM.
                // Both flag values therefore target frame_buffer at dma_offset.
                if (dma_offset + dma_this_chunk > (uint32_t)SECOND_SIGHT_FB_SIZE) {
                    command_step = 0;
                    reg_handshake = 0x00;
                    return;
                }
                setup_dma(DMA_DIRECTION_IN, frame_buffer + dma_offset, dma_this_chunk);
            } else if (command_step == 4) {
                dma_length_remaining -= dma_this_chunk;
                dma_offset += dma_this_chunk;
                if (dma_length_remaining > 0) {
                    longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                    pending_status = 0x00;
                    //command_step = 5; the longrun checker will increment this..
                } else {  // DMA complete, we're done.
                    record_upload_log(upload_code_data_flag, upload_start_offset,
                        upload_total_length, upload_host_src);
                    command_step = 0;
                    reg_handshake = 0x00;
                }
            } else if (command_step == 5) {
                dma_this_chunk = (dma_length_remaining & 0xFF0000) ? 0x1'0000 : dma_length_remaining;
                setup_dma(DMA_DIRECTION_IN, frame_buffer + dma_offset, dma_this_chunk);
                command_step = 3; // loop (will loop back to command_step == 4 because dma end will command_step++ )
            }
            /*
               $01: code/data flag (0 or 1 — firmware bug makes both go to VGA VRAM)
               $02: dest address (24-bit VRAM offset)
               $05: length (24-bit)
               $08: host read address (65816 reads; each byte written via C0B1)
            */
        }

        void cmd_set_vga_reg() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 6);
            } else if (command_step == 2) {
                // DMA complete, we're done.
                uint16_t ir = (cmd_buffer[2] << 8) | cmd_buffer[1];
                uint8_t ir_val = cmd_buffer[3];
                uint16_t ra = (cmd_buffer[5] << 8) | cmd_buffer[4];
                uint8_t rv = cmd_buffer[6];
                if (ir == 0x3D4 && ra == 0x3D5) {
                    write_crtc_reg(ir_val, rv);
                }
                /* printf("SecondSight: cmd_set_vga_reg: ir=%X, ir_val=%X, ra=%X, rv=%X (base=%X)\n",
                    ir, ir_val, ra, rv, screen_base_addr); */
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        void cmd_get_vga_reg() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 5);
            } else if (command_step == 2) {
                // args DMA complete..
                // let handshake be 0 for a bit.
                longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                pending_status = 0x00;
            } else if (command_step == 3) {
                // ok now 
                uint16_t ir = (cmd_buffer[2] << 8) | cmd_buffer[1];
                uint8_t ir_val = cmd_buffer[3];
                uint16_t ra = (cmd_buffer[5] << 8) | cmd_buffer[4];
                resp_buffer[0] = read_crtc_reg(ir_val, ra);
                setup_dma(DMA_DIRECTION_OUT, resp_buffer, 1);
            } else if (command_step == 4) {

                dma_length_remaining -= dma_this_chunk;
                dma_offset += dma_this_chunk;
                if (dma_length_remaining > 0) {
                    longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                    pending_status = 0x00;
                    //command_step = 5; the longrun checker will increment this..
                } else {  // DMA complete, we're done.
                    command_step = 0;
                    reg_handshake = 0x00;
                }
            }
        }

        void cmd_set_palette() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 3);
            } else if (command_step == 2) {
                longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                pending_status = 0x00;
            } else if (command_step == 3) {
                setup_dma(DMA_DIRECTION_IN, (uint8_t *)palette_rgb, 768);
            } else if (command_step == 4) {
                // DMA wrote 768 bytes of RGB triplets into palette_rgb.
                sync_vga_palette_from_rgb();
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        void cmd_set_palette_entry() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 3);
            } else if (command_step == 2) {
                // DMA complete, we're done.
                uint8_t index = cmd_buffer[1];
                uint8_t red = cmd_buffer[2];
                uint8_t green = cmd_buffer[3];
                uint8_t blue = cmd_buffer[4];
                set_palette_entry(index, red, green, blue);
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        void cmd_scroll_screen() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 9);
            /* }  else if (command_step == 2) {
                // args DMA complete..
                // let handshake be 0 for a bit.
                longrun_cycle_target = clock->get_cycles() + WAIT_CYCLES; // set up trigger
                pending_status = 0x00; */
            } else if (command_step == 2) {
                // DMA complete, we're done.
                uint32_t start_addr = screen_base_addr + ((cmd_buffer[3] << 16) | (cmd_buffer[2] << 8) | cmd_buffer[1]);
                uint32_t dest_addr = screen_base_addr + ((cmd_buffer[6] << 16) | (cmd_buffer[5] << 8) | cmd_buffer[4]);
                uint32_t length = (cmd_buffer[9] << 16) | (cmd_buffer[8] << 8) | cmd_buffer[7];
                printf("SecondSight: cmd_scroll_screen: start_addr=%X, dest_addr=%X, length=%X\n", start_addr, dest_addr, length);
                // we need to do the copy directionally; if we are moving bytes up (higher addr) in memory, we need to copy down; 
                // if we are moving bytes down in memory, we need to copy up.
                if (start_addr < dest_addr) {
                    for (int32_t i = length - 1; i >= 0; i--) {
                        frame_buffer[dest_addr + i] = frame_buffer[start_addr + i];
                    }
                } else {
                    for (uint32_t i = 0; i < length; i++) {
                        frame_buffer[dest_addr + i] = frame_buffer[start_addr + i];
                    }
                }
                longrun_cycle_target = clock->get_cycles() + LONGRUN_WAIT_CYCLES; // set up trigger
                pending_status = 0xA5;
                command_step = 0;
                //reg_handshake = 0x00;
            }
        }

        /* Commands with a DMA from Card to II for response data
            ==> EXECUTE DATA_OUT
            GetStatus 
        */
        /* Commands with DMA from II to card for arguments
           ==> DATA_IN EXECUTE
           SetMode
           SetUserMode
           SetPalette
           SetPaletteEntry
           SetBorder
           RunCode
           SetVGAReg
           SetShadow
           SetTextFont
           ClearScreen (*long*)
           ScrollScreen (*long*)
        */
        /* Commands */
        /* commands with two or more DMA (2; upload can have multiple chunks)
           UploadCodeData DATA_IN EXECUTE DATA_IN (*long*)
           GetVGAReg DATA_IN EXECUTE DATA_OUT (*long*)
        */
        /* Just bare commands
           ScreenOff
           ScreenOn
        */

        void cmd_screen_off() {
            display_enabled = 0;
            command_step = 0;
            reg_handshake = 0x00;
        }
        void cmd_screen_on() {
            display_enabled = 1;
            command_step = 0;
            reg_handshake = 0x00;
        }
        void cmd_set_border() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 1);
            } else if (command_step == 2) {
                // DMA complete, we're done.
                uint8_t color = cmd_buffer[1];
                //set_border(color); // TODO: we do not implement a border, or a border color, yet.
                command_step = 0;
                reg_handshake = 0x00;
            }
        }
        void cmd_set_shadow() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 1);
            } else if (command_step == 2) {
                // DMA complete, we're done.
                uint8_t shadow = cmd_buffer[1];
                //set_border(color); // TODO: we do not implement a border, or a border color, yet.
                command_step = 0;
                reg_handshake = 0x00;
            }
        }
        /*
            track active command - we do
            track command step - 0 is none; others are command specific, but increment as each step completes.
        */
        
        void execute_command() {
            switch (active_command) {
                case 0:
                    cmd_get_status();
                    break;
                case 1:
                    cmd_set_mode();
                    break;
                case 2:
                    cmd_upload_code_data();
                    break;
                case 3:
                    cmd_scroll_screen();
                    break;
                case 4:
                    cmd_screen_off();
                    break;
                case 5:
                    cmd_screen_on();
                    break;
                case 6:
                    cmd_set_palette();
                    break;
                case 7:
                    cmd_set_palette_entry();
                    break;
                case 8:
                    cmd_set_border();
                    break;
                case 9:
                    // cmd_run_code
                    // we have no code to run, so disregard.
                    break;
                case 10:
                    cmd_clear_screen();
                    break;
                case 11:
                    cmd_set_shadow();
                    break;
                case 12:
                    cmd_set_vga_reg();
                    break;
                case 13:
                    cmd_get_vga_reg();
                    break;
                case 14: 
                    cmd_set_user_mode();
                    break;
                case 15:
                    cmd_set_text_font();
                    break;
                default:
                    printf("SecondSight: execute_command: unknown command %d\n", active_command);
                    break;
            }
        }

        uint64_t longrun_cycle_target = 0;
        uint8_t pending_status = 0;

        void command_longrun_status() {
            if (longrun_cycle_target && (longrun_cycle_target < clock->get_cycles())) {
                reg_handshake = pending_status;
                longrun_cycle_target = 0;
                if (command_step) {
                    command_step++;
                    execute_command(); // TODO: having this here is icky.
                }
            }
        }

        uint8_t read_handshake() {
            command_longrun_status();

            uint8_t value;
            value = reg_handshake;
            return value;
        }

        void write_cmd(uint8_t value) {
            //reg_handshake = 0x01;

            if (dma_address) { // catch buggy library calls writing data to the cmd port 
                write_data(0, value);
                return;
            }

            active_command = value;
            cmd_buffer[0] = value;
            //cmd_length = cmd_lengths[value]-1;

            command_step = 1;
            execute_command();
        }

        // send additional data related to the command
        void write_data(uint8_t address, uint8_t value) {
            if (dma_direction == DMA_DIRECTION_OUT) {
                return;
            }
            if (dma_address) {
                *dma_address = value;
                dma_address++;
                dma_length--;
                if (dma_length == 0) {
                    dma_address = nullptr;
                    dma_length = 0;
                    reg_handshake = 0x00;
                    if (command_step) {
                        command_step++;
                        execute_command();
                    }
                }
            }
        }

        // Model "long running command" by defining what apple II cycle the handshake
        // will complete and return 0xA5 or 0xA6 at.

        uint8_t read_data() {
            if (dma_direction == DMA_DIRECTION_IN) {
                return 0;
            }
            // so DMA_DIRECTION_OUT
            uint8_t value = 0;
            if (dma_address) {
                value = *dma_address;
                dma_address++;
                dma_length--;
                if (dma_length == 0) {
                    dma_address = nullptr; // end DMA
                    reg_handshake = 0x00;
                    if (command_step) {
                        command_step++;
                        execute_command();
                    }
                }
            }
            return value;
        }


        bool frame() {
            if (!vga_active) {
                return false;
            }
            if (!display_enabled) {
                return true; // we control frame, but have nothing to draw.
            }
            const uint8_t *display_base = frame_buffer + screen_base_addr;
            if (is_text_mode()) {
                const int text_pitch = fb_pitch > 0 ? fb_pitch : VGA_TEXT_FB_PITCH;
                if (text_font_index != 3) {
                    std::string font_path;
                    Paths::calc_base(font_path, "img/IBM_VGA_8x16.png");
                    vga_text_9x16_init(font_path.c_str());
                }
                if (tex_text) {
                    vga_render_text_9x16(vs, tex_text, display_base, text_pitch);
                }
            } else if (current_vga_mode.color_depth == 8) {
                vga_render_8bpp(vs, tex_24bpp, rgb24_buffer, palette_rgb, display_base, fb_pitch,
                    current_vga_mode.width, current_vga_mode.height);
            } else if (current_vga_mode.color_depth == 16) {
                vga_render_16bpp(vs, tex_16bpp, display_base, fb_pitch,
                    current_vga_mode.width, current_vga_mode.height);
            } else if (current_vga_mode.color_depth == 24) {
                vga_render_24bpp(vs, tex_24bpp, display_base, fb_pitch,
                    current_vga_mode.width, current_vga_mode.height);
            }
            return true;
        }

        void debug_dump_vram_at(DebugFormatter *df, uint32_t byte_offset) const {
            if (frame_buffer == nullptr) {
                df->addLine("VRAM: (no buffer)");
                return;
            }
            if (byte_offset >= SECOND_SIGHT_FB_SIZE) {
                df->addLine("VRAM: base %X out of range", byte_offset);
                return;
            }

            const int pitch = fb_pitch > 0 ? fb_pitch
                : (is_text_mode() ? current_vga_mode.width * VGA_TEXT_CELL_BYTES : 80);
            const int row_count = is_text_mode() ? 4 : 2;
            const uint8_t *base = frame_buffer + byte_offset;

            df->addLine("--- VRAM @ +%05X pitch %d (%d rows) ---",
                byte_offset, pitch, row_count);

            char line_buf[160];
            for (int row = 0; row < row_count; row++) {
                const size_t row_byte_off = (size_t)row * (size_t)pitch;
                if (byte_offset + row_byte_off + (size_t)pitch > SECOND_SIGHT_FB_SIZE) {
                    df->addLine("VRAM: row %d exceeds buffer", row);
                    break;
                }
                const uint8_t *row_ptr = base + row_byte_off;

                for (int col = 0; col < pitch; col += 16) {
                    int chunk = pitch - col;
                    if (chunk > 16) {
                        chunk = 16;
                    }
                    int pos = snprintf(line_buf, sizeof(line_buf), "+%04X:",
                        row * pitch + col);
                    for (int i = 0; i < chunk; i++) {
                        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                            " %02X", row_ptr[col + i]);
                    }
                    df->addLine("%s", line_buf);
                }

                if (is_text_mode()) {
                    const int cells = pitch / VGA_TEXT_CELL_BYTES;
                    const int show_cells = cells < 40 ? cells : 40;
                    int pos = snprintf(line_buf, sizeof(line_buf), "  r%02d", row);
                    for (int c = 0; c < show_cells; c++) {
                        const uint8_t ch = row_ptr[c * VGA_TEXT_CELL_BYTES];
                        const uint8_t at = row_ptr[c * VGA_TEXT_CELL_BYTES + 1];
                        char disp = (ch >= 0x20 && ch < 0x7F) ? (char)ch : '.';
                        pos += snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos,
                            " %c:%02X", disp, at);
                    }
                    if (cells > show_cells) {
                        snprintf(line_buf + pos, sizeof(line_buf) - (size_t)pos, " ...");
                    }
                    df->addLine("%s", line_buf);
                }
            }
        }

        void debug_dump_vram(DebugFormatter *df) const {
            debug_dump_vram_at(df, screen_base_addr);
        }

        void debug_scan_text_regions(DebugFormatter *df) const {
            if (frame_buffer == nullptr || !is_text_mode()) {
                return;
            }
            static const uint32_t probes[] = {
                0, 0x400, 0x800, 0x1000, 0x1020, 0x2000, 0xB800,
            };
            df->addLine("Text probes (printable ch, row0):");
            for (uint32_t off : probes) {
                if (off + VGA_TEXT_FB_PITCH > SECOND_SIGHT_FB_SIZE) {
                    continue;
                }
                int score = 0;
                for (int c = 0; c < 40; c++) {
                    const uint8_t ch = frame_buffer[off + c * 2];
                    const uint8_t at = frame_buffer[off + c * 2 + 1];
                    if (ch >= 0x20 && ch < 0x7F && at < 0x80) {
                        score++;
                    }
                }
                df->addLine("  +%05X: %d/40", off, score);
            }
        }

        void debug_upload_log(DebugFormatter *df) const {
            if (upload_small_total > 0) {
                df->addLine("Uploads <%u B: %u (omitted from log)",
                    SS_UPLOAD_LOG_PRINTF_MIN, upload_small_total);
            }
            const int show = upload_log_total < SS_UPLOAD_LOG_MAX ? upload_log_total : SS_UPLOAD_LOG_MAX;
            df->addLine("Upload log (bulk, last %d):", show);
            for (int i = 0; i < show; i++) {
                const int idx = (upload_log_next - 1 - i + SS_UPLOAD_LOG_MAX * 2) % SS_UPLOAD_LOG_MAX;
                const upload_log_entry_t &e = upload_log[idx];
                df->addLine("  VRAM %06X len=%u host=%06X (flag=%d)",
                    e.dest, e.length, e.host_src, e.code_data_flag);
            }
        }

        void debug(DebugFormatter *df) {
            df->addLine("VGA Active: %d", vga_active);
            df->addLine("VGA Mode Num: %02X", vga_mode_num);
            if (text_font_index == 0xFF) {
                df->addLine("TextFont: (not set)");
            } else {
                df->addLine("TextFont: %02X %s", text_font_index, text_font_label(text_font_index));
            }
            df->addLine("Screen Base Addr: %X", screen_base_addr);
            df->addLine("CRT Start Addr: %02X%02X", crt_start_addr_high, crt_start_addr_low);
            df->addLine("CRT HDisplay End: %02X (+%d clocks)", crt_hdisplay_end, crtc_char_clocks_per_line());
            df->addLine("CRT Addr Scale: %u bytes/unit", crtc_bytes_per_address_unit());
            df->addLine("FB Pitch: %d", fb_pitch);
            df->addLine("Res X/Y @ Depth: %d/%d @ %d", res_x, res_y, current_vga_mode.color_depth);
            df->addLine("Active command: %d", active_command);
            debug_upload_log(df);
            if (z180_sram != nullptr) {
                df->addLine("Z180 $F000[0..3]: %02X %02X %02X %02X",
                    z180_sram[SS_Z180_USER_FONT_ADDR], z180_sram[SS_Z180_USER_FONT_ADDR + 1],
                    z180_sram[SS_Z180_USER_FONT_ADDR + 2], z180_sram[SS_Z180_USER_FONT_ADDR + 3]);
            }
            //debug_scan_text_regions(df);
            //debug_dump_vram_at(df, screen_base_addr);
        }
};

void init_secondsight(computer_t *computer, SlotType_t slot);