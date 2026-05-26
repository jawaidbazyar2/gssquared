#pragma once
#include <cstdint>
#include <cstring>
#include "util/DebugFormatter.hpp"
#include "gs2.hpp"
#include "videosystem.hpp"
#include "NClock.hpp"

struct computer_t;


#define SECOND_SIGHT_FB_SIZE 1024 * 1024

static constexpr int SS_MAX_WIDTH = 1024;
static constexpr int SS_MAX_HEIGHT = 768;
static constexpr int SS_RGB24_PITCH = SS_MAX_WIDTH * 3;
static constexpr size_t SS_RGB24_BUFFER_SIZE = SS_RGB24_PITCH * SS_MAX_HEIGHT;

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

    SDL_Color vga_palette[256] = {};
    uint8_t palette_rgb[256][3] = {};
    uint8_t *rgb24_buffer = nullptr;

    uint8_t crt_start_addr_high = 0; // CRTC reg 0x0C
    uint8_t crt_start_addr_low = 0;  // CRTC reg 0x0D
    uint32_t screen_base_addr = 0;
    uint16_t fb_pitch = 0;

    void update_screen_base_addr() {
        screen_base_addr = ((uint32_t)crt_start_addr_high << 8) | crt_start_addr_low;
    }

    void write_crtc_reg(uint8_t index, uint8_t value) {
        switch (index) {
            case 0x0C:
                crt_start_addr_high = value;
                update_screen_base_addr();
                break;
            case 0x0D:
                crt_start_addr_low = value;
                update_screen_base_addr();
                break;
        }
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

    void expand_8bpp_to_rgb24(const uint8_t *src, int src_pitch, int width, int height) {
        for (int y = 0; y < height; y++) {
            const uint8_t *row = src + (y * src_pitch);
            uint8_t *out = rgb24_buffer + (y * SS_RGB24_PITCH);
            for (int x = 0; x < width; x++) {
                const uint8_t *rgb = palette_rgb[row[x]];
                out[x * 3 + 0] = rgb[0];
                out[x * 3 + 1] = rgb[1];
                out[x * 3 + 2] = rgb[2];
            }
        }
    }

    public:
        SecondSight(video_system_t *video_system, MMU *mmu, NClock *clock) : vs(video_system), mmu(mmu), clock(clock) {
            frame_buffer = new uint8_t[SECOND_SIGHT_FB_SIZE];
            //memset(frame_buffer, 0, SECOND_SIGHT_FB_SIZE); // clear the buffer
            for (int i = 0; i < SECOND_SIGHT_FB_SIZE; i++) {
                frame_buffer[i] = i & 0xFF;
            }

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
        }
        ~SecondSight() {
            delete[] frame_buffer;
            delete[] rgb24_buffer;
            SDL_DestroyTexture(tex_16bpp);
            SDL_DestroyTexture(tex_24bpp);
        }

        void reset() {
            dma_address = nullptr;
            dma_length = 0;

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
            screen_base_addr = 0;
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
            bool emulated;
            tg_t graphics;
            uint16_t width;
            uint16_t height;
            uint8_t color_depth; // in bits
            uint8_t bitspercolor;
        };

        static constexpr vga_mode_t vga_modes[] = {
            {0x13, true, TG_GRAPHICS, 320, 200, 8, 6},
            {0x53, true, TG_GRAPHICS, 640, 480, 8, 8},
            {0x5C, true, TG_GRAPHICS, 640, 480, 16, 5},
            {0x5F, true, TG_GRAPHICS, 640, 480, 24, 8},
            {0x61, true, TG_GRAPHICS, 640, 400, 8, 8},
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
        current_vga_mode.emulated = true;
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
                setup_dma(DMA_DIRECTION_IN, cmd_buffer, 0x02);
            } else if (command_step == 2) {
                if (cmd_buffer[0] == 0xFF) {
                    mode_info info;
                    analyze_vga_mode(&user_mode_rec, &info);
                    apply_analyzed_mode(&user_mode_rec, &info);
                } else {
                    for (int i = 0; i < sizeof(vga_modes) / sizeof(vga_mode_t); i++) {
                        if (vga_modes[i].mode == cmd_buffer[0]) {
                            current_vga_mode = vga_modes[i];
                            vga_active = vga_modes[i].emulated;
                            vga_mode_num = cmd_buffer[0];
                            res_x = current_vga_mode.width;
                            res_y = current_vga_mode.height;
                            fb_pitch = current_vga_mode.width * (current_vga_mode.color_depth / 8);
                            break;
                        }
                    }
                }
                longrun_cycle_target = clock->get_cycles() + 50; // set up trigger
                pending_status = 0xA5;
            }
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
                longrun_cycle_target = clock->get_cycles() + 50; // set up trigger
                pending_status = 0xA5;
            }
        }


        /*
        1: DMA_IN - get arguments
        2: EXECUTE
        3: DMA_IN - get data to load!
        4: DONE
        */
        void cmd_upload_code_data() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 10);
            } else if (command_step == 2) {
                // args DMA complete..
                // let handshake be 0 for a bit.
                longrun_cycle_target = clock->get_cycles() + 50; // set up trigger
                pending_status = 0x00;
            } else if (command_step == 3) {
                // ok now 
                uint8_t code_data_flag = cmd_buffer[1];
                uint32_t address = (cmd_buffer[4] << 16) | (cmd_buffer[3] << 8) | cmd_buffer[2];
                uint32_t length = (cmd_buffer[7] << 16) | (cmd_buffer[6] << 8) | cmd_buffer[5];
                
                setup_dma(DMA_DIRECTION_IN, frame_buffer+address, length);
            } else if (command_step == 4) {
                // DMA complete, we're done.
                command_step = 0;
                reg_handshake = 0x00;
            }
            /* 
               $01: code/data flag (0 = code, 1 = data i.e. frame buffer)
                    data = frame_buffer
               $02: $0ABBCC: adddress to write to
               $05: $0LLLLL: length of data
               $08: $AABBCC: IIGS address to take data from
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
                printf("SecondSight: cmd_set_vga_reg: ir=%X, ir_val=%X, ra=%X, rv=%X (base=%X)\n",
                    ir, ir_val, ra, rv, screen_base_addr);
                command_step = 0;
                reg_handshake = 0x00;
            }
        }

        void cmd_set_palette() {
            if (command_step == 1) {
                setup_dma(DMA_DIRECTION_IN, cmd_buffer+1, 3);
            } else if (command_step == 2) {
                longrun_cycle_target = clock->get_cycles() + 50; // set up trigger
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
        /* commands with two DMA (sometimes 2 )
           UploadCodeData DATA_IN EXECUTE DATA_IN (*long*)
           GetVGAReg DATA_IN EXECUTE DATA_OUT (*long*)
        */
        /* Just bare commands
           ScreenOff
           ScreenOn
        */

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
                case 6:
                    cmd_set_palette();
                    break;
                case 10:
                    cmd_clear_screen();
                    break;
                case 12:
                    cmd_set_vga_reg();
                    break;
                case 14: 
                    cmd_set_user_mode();
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
                reg_handshake = pending_status; // 0xA5; // TODO: or error
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
            // draw the frame
            SDL_FRect src = { 0.0f, 0.0f, (float)current_vga_mode.width, (float)current_vga_mode.height };
            const uint8_t *display_base = frame_buffer + screen_base_addr;

            // update the texture based on what's in frame based on specified resolution and color depth.
            if (current_vga_mode.color_depth == 8) {
                expand_8bpp_to_rgb24(
                    display_base,
                    fb_pitch,
                    current_vga_mode.width,
                    current_vga_mode.height);
                SDL_UpdateTexture(tex_24bpp, nullptr, rgb24_buffer, SS_RGB24_PITCH);
                vs->render_frame(tex_24bpp, &src, nullptr);
            } else if (current_vga_mode.color_depth == 16) {
                SDL_UpdateTexture(tex_16bpp, nullptr, display_base, fb_pitch);
                vs->render_frame(tex_16bpp, &src, nullptr);
            } else if (current_vga_mode.color_depth == 24) {
                SDL_UpdateTexture(tex_24bpp, nullptr, display_base, fb_pitch);
                vs->render_frame(tex_24bpp, &src, nullptr);
            }
            return true;
        }

        void debug(DebugFormatter *df) {

        }
};

void init_secondsight(computer_t *computer, SlotType_t slot);