#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <SDL3/SDL.h>

//#include "devices/displaypp/frame/frame_bit.hpp"
#include "SDL3/SDL_keycode.h"
#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_surface.h"
#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/VideoScannerIIe.hpp"
#include "devices/displaypp/VideoScannerIIgs.hpp"
#include "devices/displaypp/VideoScanGenerator_RGB.hpp"
#include "devices/displaypp/VideoScanGenerator_Comp.hpp"
#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
//#include "devices/displaypp/render/GSRGB560.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/ScanBuffer.hpp"
#include "mmus/mmu_iie.hpp"
#include "util/printf_helper.hpp"

#define ASPECT_RATIO (1.28f)
#define SCALE_X 2
#define SCALE_Y (4*ASPECT_RATIO)
#define XY_RATIO (SCALE_Y / SCALE_X)

struct canvas_t {
    float w;
    float h;
};

int text_addrs[24] =
{   // text page 1 line addresses
    0x0000,
    0x0080,
    0x0100,
    0x0180,
    0x0200,
    0x0280,
    0x0300,
    0x0380,

    0x0028,
    0x00A8,
    0x0128,
    0x01A8,
    0x0228,
    0x02A8,
    0x0328,
    0x03A8,

    0x0050,
    0x00D0,
    0x0150,
    0x01D0,
    0x0250,
    0x02D0,
    0x0350,
    0x03D0,
};

void generate_dlgr_test_pattern(uint8_t *textpage, uint8_t *altpage) {

    int c = 0;

    for (int x = 0; x <= 79; x++) {
        
        // if x is even, use altpage.
        // if x is odd, use textpage.
        // basic program does:
        // poke C054 + (x is odd)
        uint8_t *addr = (x % 2) ? altpage : textpage;

        for (int y = 0; y < 24; y ++) {

            uint8_t *laddr = addr + text_addrs[y] + (x/2);
            uint8_t val = 0;

            // if y is even, modify bottom nibble.
            val = (c & 0x0F);
            if (++c > 15) c = 0;
            
            // if y is odd, modify top nibble.
            val |= (c << 4);
            if (++c > 15) c = 0;

            // store it
            *laddr = val;
            
        }
    }
}

#define II_SCREEN_TEXTURE_WIDTH (580)
#define II_SCREEN_TEXTURE_HEIGHT (192)

#define SCREEN_TEXTURE_WIDTH (567)
#define SCREEN_TEXTURE_HEIGHT (192)

#define SCANNER_II 1
#define SCANNER_IIE 2
#define SCANNER_IIGS 3

struct border_rect_t {
    SDL_FRect src;
    SDL_FRect dst;
};

typedef border_rect_t border_rect_array_t[3][3];

/* border_rect_array_t ii_borders; // [y][x]
border_rect_array_t shr_borders; // [y][x] */

#define B_TOP 0
#define B_CEN 1
#define B_BOT 2
#define B_LT 0
#define B_RT 2

/*
border texture is laid out based on the hc/vc positions. i.e
   0-6: right border
   7-12: left border
   13-52: top/bottom border center content

*/ 
struct display_state_t {
    border_rect_array_t ii_borders;
    border_rect_array_t shr_borders;
};


#if 0
void print_border_rects() {
    /* print_rect("ii_borders[B_TOP][B_LT]", ii_borders[B_TOP][B_LT]);
    print_rect("ii_borders[B_TOP][B_CEN]", ii_borders[B_TOP][B_CEN]);
    print_rect("ii_borders[B_TOP][B_RT]", ii_borders[B_TOP][B_RT]);
    print_rect("ii_borders[B_CEN][B_LT]", ii_borders[B_CEN][B_LT]); */
    print_rect("ii_borders[B_CEN][B_CEN]", ii_borders[B_CEN][B_CEN]);
    /* print_rect("ii_borders[B_CEN][B_RT]", ii_borders[B_CEN][B_RT]);
    print_rect("ii_borders[B_BOT][B_LT]", ii_borders[B_BOT][B_LT]);
    print_rect("ii_borders[B_BOT][B_CEN]", ii_borders[B_BOT][B_CEN]);
    print_rect("ii_borders[B_BOT][B_RT]", ii_borders[B_BOT][B_RT]); */
}
#endif

bool readFile(const char *path, uint8_t *data, size_t size) {
    FILE *f3 = fopen(path, "rb");
    if (!f3) {
        printf("Failed to load file: %s\n", path);
        return false;
    }
    fread(data, 1, size, f3);
    fclose(f3);
    return true;
}

void print_canvas(const char *name, canvas_t *c) {
    printf("%s: (%f, %f)\n", name, c->w, c->h);
}

bool calculateScale(SDL_Renderer *renderer, canvas_t &c, canvas_t &s) {
    
    /* C_ASPECT = 1.28
    xscale = canvas.w / source.w
    yheight = canvas.w / canvas.aspect
    yscale = yheight / source.h */

    float new_scale_x = c.w / s.w;
    float new_y_height = c.w / ASPECT_RATIO;
    float new_scale_y = new_y_height / 200 /* s.h */;

    // now we want to constrain the Y scale to the aspect ratio target.

    float scale_ratio = new_scale_y / new_scale_x;
    print_canvas("c", &c);
    print_canvas("s", &s);
    printf("window_resize: new w/h (%f, %f) -> (%f, %f) scale ratio: %f\n", c.w, c.h, new_scale_x, new_scale_y, scale_ratio);
    return SDL_SetRenderScale(renderer, new_scale_x, new_scale_y);
}

// manually set window size
bool setWindowSize(SDL_Window *window, SDL_Renderer *renderer, canvas_t &c, canvas_t &s) {

    float new_aspect = (float)c.w / c.h;
    printf("setWindowSize: (%f, %f) @ %f\n", c.w, c.h, new_aspect);
    
    bool res = SDL_SetWindowSize(window, c.w, c.h);
    if (!res) {
        return false;
    }
    return calculateScale(renderer, c, s);
}

// handle window resize - user resized.
bool window_resize(const SDL_Event &event, canvas_t &s, SDL_Window *window, SDL_Renderer *renderer) {

    canvas_t c = { (float)event.window.data1, (float)event.window.data2 };

    return calculateScale(renderer, c, s);
}

void copyToMMU(MMU_IIe *mmu, uint8_t *data, uint32_t addr, uint32_t size) {
    memcpy(mmu->get_memory_base() + addr, data, size);
}

int main(int argc, char **argv) {
    SDL_ScaleMode scales[3] = {SDL_SCALEMODE_LINEAR, SDL_SCALEMODE_NEAREST, SDL_SCALEMODE_PIXELART,  };
    //border_rect_array_t *modes_rects[2] = { ds.ii_borders, ds.shr_borders };

    // hsize and vsize:
    // left and right arrows stretch (right) or shrink (left) the size of viewport; based on center of 910.
    // up and down arrows stretch (up) or shrink (down) the size of the viewport; based on center of 263.

    /* int32_t vsize = 237; // initial size
    int32_t hsize = 680; // initial size
    int32_t hpos = -8; // initial position
    int32_t vpos = 4; // initial position */
    int32_t vsize = 0;
    int32_t hsize = 0;
    int32_t hpos = 0;
    int32_t vpos = 0;
    bool frame_moved = true;
    SDL_FRect ii_frame_src;

    uint64_t start = 0, end = 0;

    canvas_t canvasses[2] = {
        { (float)1160, (float)906 },
        {  (float)1280, (float)1000 }
    };
    canvas_t sources[9] = {
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // UNUSED
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 40 text
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 80 text
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 40 lores
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 80 lores
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 40 hires
        { (float)II_SCREEN_TEXTURE_WIDTH, (float)192 }, // 80 hires
        { (float)640, (float)200 }, // shr
        { (float)640, (float)200 } // shr
    };

    uint8_t *rom = new uint8_t[12*1024];

    MMU_IIe *mmu = new MMU_IIe(128, 128*1024, rom);

    // Calculate various dimensions

    //uint16_t b_w = 184 / 2;
    //uint16_t b_h = 40;
    //const uint16_t f_w = SCREEN_TEXTURE_WIDTH + b_w, f_h = SCREEN_TEXTURE_HEIGHT + b_h;
    //const uint16_t c_w = f_w * 2, c_h = f_h * 4;

    // same window size as the emulator.
    const uint16_t c_w = 1288, c_h = 928;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("VideoScanner Test Harness", c_w, c_h, SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Failed to create window\n");
        return 1;
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Failed to create renderer\n");
        return 1;
    }
    /* SDL_Texture *texture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 567, II_SCREEN_TEXTURE_HEIGHT);
    if (!texture) {
        printf("Failed to create texture\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    } */
    /* SDL_Texture *txt_border = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 53, 263);
    if (!txt_border) {
        printf("Failed to create texture\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    } */
    /* SDL_Texture *txt_shr = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 640, 200);
    if (!txt_shr) {
        printf("Failed to create txt_shr\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    } */

    // used for partial updates, screen capture.
    SDL_Texture *stage2 = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_TARGET, 910, 263);
    if (!stage2) {
        printf("Failed to create txt_shr\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED)) {
        printf("Failed to set render vsync\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    const char *rname = SDL_GetRendererName(renderer);
    printf("Renderer: %s\n", rname);
    //SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    SDL_SetRenderScale(renderer, 2.0f, 4.0f); // this means our coordinate system is 1x1 according to Apple II scanlines/pixels etc.
    int error = SDL_SetRenderTarget(renderer, nullptr);

    int testiterations = 10000;

    // *frame_byte = new(std::align_val_t(64)) Frame560(560, II_SCREEN_TEXTURE_HEIGHT);

    uint8_t *ram = mmu->get_memory_base(); //new uint8_t[0x20000]; // 128k!

    uint8_t *text_page = ram + 0x00400;
    uint8_t *alt_text_page = ram + 0x10400;
    for (int i = 0; i < 1024; i++) {
        text_page[i] = i & 0xFF;
        alt_text_page[i] = (i+1) & 0xFF;
    }

    uint8_t *lores_page = ram + 0x00800;
    uint8_t *alt_lores_page = ram + 0x10800;
    generate_dlgr_test_pattern(lores_page, alt_lores_page);

    /* -- */
    uint8_t *testhgrpic = new(std::align_val_t(64)) uint8_t[8192];
    //uint8_t *testhgrpic = ram + 0x04000;
    bool res = readFile("/Users/bazyar/src/hgrdecode/HIRES/APPLE", testhgrpic, 8192);
    if (!res) {
        printf("Failed to load testhgrpic\n");
        return 1;
    }
    copyToMMU(mmu, testhgrpic, 0x04000, 8192);

    uint8_t *testdhgrpic = new(std::align_val_t(64)) uint8_t[16386];
    res = readFile("/Users/bazyar/src/hgrdecode/DHIRES/LOGO.DHGR", testdhgrpic, 16384);
    if (!res) {
        printf("Failed to load testdhgrpic\n");
        return 1;
    }

    uint8_t *testshrpic = new(std::align_val_t(64)) uint8_t[32768];
    res = readFile("/Users/bazyar/src/hgrdecode/SHR/AIRBALL", testshrpic, 32768);
    if (!res) {
        printf("Failed to load testshrpic\n");
        return 1;
    }

    uint8_t *testshrpic2 = new(std::align_val_t(64)) uint8_t[32768];
    res = readFile("/Users/bazyar/src/hgrdecode/SHR/desktop", testshrpic2, 32768);
    if (!res) {
        printf("Failed to load testshrpic\n");
        return 1;
    }

    CharRom iiplus_rom("resources/roms/apple2_plus/char.rom");
    CharRom iie_rom("resources/roms/apple2e_enh/char.rom");

    if (!iiplus_rom.is_valid() || !iie_rom.is_valid()) {
        printf("Failed to load char roms\n");
        return 1;
    }

    Monochrome560 monochrome;
    NTSC560 ntsc_render;

    uint16_t border_color = 0x0F;

    VideoScannerII *video_scanner_ii = new VideoScannerII(mmu);
    video_scanner_ii->initialize();
    VideoScannerIIe *video_scanner_iie = new VideoScannerIIe(mmu);
    video_scanner_iie->initialize();
    VideoScannerIIgs *video_scanner_iigs = new VideoScannerIIgs(mmu);
    video_scanner_iigs->initialize();
    video_scanner_iigs->set_border_color(0x0F);

    FrameVSG *fr_vsg = new(std::align_val_t(64)) FrameVSG(910, 263, renderer, PIXEL_FORMAT);
    fr_vsg->clear(RGBA_t::make(0xE0, 0x00, 0x00, 0xFF));
    VideoScanGenerator_RGB *vsgr = new VideoScanGenerator_RGB(&iie_rom, false, fr_vsg);
    VideoScanGenerator_Comp *vsgc = new VideoScanGenerator_Comp(&iie_rom, false, fr_vsg);
    VideoScanGeneratorIntf *vsg = vsgr;

    vsgc->set_render(&ntsc_render);

    vsg->set_display_shift(false);
    vsgc->set_display_shift(false);
    ntsc_render.set_shift_enabled(false);
    monochrome.set_shift_enabled(false);
    
    display_state_t ds;

    int pitch;
    void *pixels;

    uint64_t cumulative = 0;
    uint64_t times[900];
    uint64_t framecnt = 0;

    int render_mode = 3;
    int sharpness = 0;
    bool exiting = false;
    bool flash_state = false;
    int flash_count = 0;
    int scanner_choice = SCANNER_IIGS;
    int old_scanner_choice = -1;
    
    bool rolling_border = false;
    int border_cycles = 0;
    bool border_is_hc = false;
    bool border_is_vc = false;

    int generate_mode = 1;
    int last_generate_mode = -1;
    
    int last_canvas_mode = -1;
    int canvas_mode = 0;
    int fg = 0x0F;
    int bg = 0x00;

    int cycle_mode = 4; // do not process cycles. 1 = single cycle; 2 = 65 cycles.
    int vidcycle = 0;

    while (++framecnt && !exiting)  {
        VideoScannerII *scanner;
        
        uint64_t frame_start = SDL_GetTicksNS();

        if (old_scanner_choice != scanner_choice) {
            if (scanner_choice == SCANNER_II) scanner = video_scanner_ii;
            else if (scanner_choice == SCANNER_IIE) scanner = video_scanner_iie;
            else if (scanner_choice == SCANNER_IIGS) scanner = video_scanner_iigs;
            old_scanner_choice = scanner_choice;
        }

        if ((last_canvas_mode != canvas_mode) || (last_generate_mode != generate_mode)) {
            last_canvas_mode = canvas_mode;
            last_generate_mode = generate_mode;

            switch (generate_mode) {
                case 6: 
                    copyToMMU(mmu, testdhgrpic, 0x12000, 8192); // aux is first
                    copyToMMU(mmu, testdhgrpic+0x2000, 0x02000, 8192);
                    break;
                case 7:
                    copyToMMU(mmu, testshrpic, 0x12000, 32768);
                    break;
                case 8:
                    copyToMMU(mmu, testshrpic2, 0x12000, 32768);
                    break;
            }           
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exiting = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                switch (event.key.key) { 
                    case SDLK_F8:
                        //vsg->setDumpNextFrame(true);
                        break;
                    case SDLK_1:
                        frame_moved = true;
                        generate_mode = 1;
                        scanner->set_page_1();
                        scanner->reset_80col();
                        scanner->set_text();
                        scanner->reset_shr();
                        break;
                
                    case SDLK_2:
                        frame_moved = true;
                        generate_mode = 2;
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->set_text();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_3:
                        frame_moved = true;
                        generate_mode = 3;
                        scanner->set_page_1();
                        scanner->set_graf();
                        scanner->reset_80col();
                        scanner->set_lores();
                        scanner->reset_dblres();
                        scanner->reset_shr();
                        break;

                    case SDLK_D:
                        
                        scanner->set_dblres_f(!scanner->is_dblres());
                        
                        break;

                    case SDLK_4:
                        frame_moved = true;
                        generate_mode = 4;
                        scanner->set_graf();
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->set_lores();
                        scanner->set_dblres();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_5:
                        frame_moved = true;
                        generate_mode = 5;
                        scanner->set_page_2();
                        scanner->reset_80col();
                        scanner->set_graf();
                        scanner->set_hires();
                        scanner->reset_dblres();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_6:
                        frame_moved = true;
                        generate_mode = 6;
                        scanner->set_dblres();
                        scanner->set_hires();
                        scanner->set_graf();
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_7:
                        frame_moved = true;
                        generate_mode = 7;
                        scanner->set_shr();
                        break;
                    
                    case SDLK_8:
                        frame_moved = true;
                        generate_mode = 8;
                        scanner->set_shr();
                        break;
                    
                    case SDLK_N:
                        render_mode = 2;
                        vsgc->set_render(&ntsc_render);
                        SDL_SetTextureColorMod(fr_vsg->get_texture(), 0xFF, 0xFF, 0xFF);
                        break;
                    
                    case SDLK_M:
                        if (event.key.mod & SDL_KMOD_SHIFT) {
                            vsg->set_mono_mode(!vsg->get_mono_mode());
                            vsgc->set_mono_mode(!vsgc->get_mono_mode());
                        } else {
                            render_mode = 1;
                            vsgc->set_render(&monochrome);
                            SDL_SetTextureColorMod(fr_vsg->get_texture(), 0x00, 0xFF, 0x00);
                        }
                        break;
                    
                    case SDLK_R:
                        render_mode = 3;
                        SDL_SetTextureColorMod(fr_vsg->get_texture(), 0xFF, 0xFF, 0xFF);
                        break;

                    case SDLK_A:
                        scanner->set_altchrset();
                        break;
                    
                    case SDLK_Z:
                        scanner->reset_altchrset();
                        break;
                    
                    case SDLK_S:
                        scanner->set_mixed();
                        break;
                    
                    case SDLK_X:
                        scanner->set_full();
                        break;
                    
                    case SDLK_P:
                        sharpness = (sharpness + 1) % 3;
                        SDL_SetTextureScaleMode(fr_vsg->get_texture(), scales[sharpness]);
                        
                        //SDL_SetTextureScaleMode(txt_shr, scales[sharpness]);
                        printf("Sharpness: %d\n", sharpness);
                        break;
                    
                    case SDLK_B:
                        border_color = (border_color + 1) & 0x0F;
                        scanner->set_border_color(border_color);
                        break;
                    case SDLK_O:
                        border_is_hc = !border_is_hc;
                        break;
                    case SDLK_I:
                        border_is_vc = !border_is_vc;
                        break;
        
                    case SDLK_C: // toggle dhr mono mode
                        vsg->set_dhgr_mono_mode(!vsg->get_dhgr_mono_mode());
                        vsgc->set_dhgr_mono_mode(!vsgc->get_dhgr_mono_mode());
                        break;
                    case SDLK_F:
                        fg = (fg + 1) & 0x0F;
                        scanner->set_text_fg(fg);
                        break;

                    case SDLK_G:
                        bg = (bg + 1) & 0x0F;
                        scanner->set_text_bg(bg);
                        break;
                    
                    case SDLK_V:
                        rolling_border = !rolling_border;
                        break;
                    
                    case SDLK_F1:
                        scanner_choice = SCANNER_II;
                        vsg->set_display_shift(false);
                        vsgc->set_display_shift(false);
                        ntsc_render.set_shift_enabled(false);
                        monochrome.set_shift_enabled(false);

                        frame_moved = true;
                        printf("Scanner choice: II\n");
                        break;
                    
                    case SDLK_F2:
                        scanner_choice = SCANNER_IIE;
                        vsg->set_display_shift(true);
                        vsgc->set_display_shift(true);
                        ntsc_render.set_shift_enabled(true);
                        monochrome.set_shift_enabled(true);

                        frame_moved = true;
                        printf("Scanner choice: IIe\n");
                        break;
                    
                    case SDLK_F3:
                        scanner_choice = SCANNER_IIGS;
                        vsg->set_display_shift(false);
                        vsgc->set_display_shift(false);
                        ntsc_render.set_shift_enabled(false);
                        monochrome.set_shift_enabled(false);

                        frame_moved = true;
                        printf("Scanner choice: IIgs\n");
                        break;
                    case SDLK_SPACE:
                        frame_moved = true;
                        if (event.key.mod & SDL_KMOD_SHIFT) {
                            cycle_mode = 2;
                        } else if (event.key.mod & SDL_KMOD_LALT) {
                            cycle_mode = 3;
                        } else if (event.key.mod & SDL_KMOD_RALT) {
                            cycle_mode = 4;
                        } else {
                            cycle_mode = 1;
                        }
                        break;
                    case SDLK_LEFT:
                        if (event.key.mod & SDL_KMOD_SHIFT) hsize -= 2;
                        else hpos += 2;
                        frame_moved = true;
                        break;
                    case SDLK_RIGHT:
                        if (event.key.mod & SDL_KMOD_SHIFT) hsize += 2;
                        else hpos -= 2;
                        frame_moved = true;
                        break;
                    case SDLK_UP:
                        if (event.key.mod & SDL_KMOD_SHIFT) vsize -= 2;
                        else vpos += 2;
                        frame_moved = true;
                        break;
                    case SDLK_DOWN:
                        if (event.key.mod & SDL_KMOD_SHIFT) vsize += 2;
                        else vpos -= 2;
                        frame_moved = true;
                        break;
                    case SDLK_HOME:
                        hsize = 0; vsize = 0;
                        hpos = 0; vpos = 0;
                        
                        frame_moved = true;
                        break;
                    case SDLK_END:
                        printf("hsize: %d vsize: %d hpos: %d vpos: %d\n", hsize, vsize, hpos, vpos);
                        break;
                }
                printf("key pressed: %d\n", event.key.key);
            }
        }

        int phaseoffset = 1; // now that I start normal (40) display at pixel 7, its phase is 1 also. So, both 40 and 80 display start at phase 1 now.
        ScanBuffer *scanbuf = nullptr;
        
        /* exactly one frame worth of video cycles */
        if (cycle_mode) {
            int num_cycles;
            switch (cycle_mode) {
                case 1:
                    num_cycles = 1;
                    break;
                case 2:
                    num_cycles = 65;
                    break;
                case 3:
                case 4:
                    // these should sync up partial frame so frame after is fully correct.
                    num_cycles = 17030;
                    break;
            }
            for (int i = 0; i < num_cycles; i++) {

                if ((rolling_border) && (border_cycles == 13)) {
                    border_color = (border_color + 1) & 0x0F;
                    fg = (fg + 1) & 0x0F;
                    bg = (bg + 1) & 0x0F;
                    scanner->set_text_fg(fg);
                    scanner->set_text_bg(bg);
                    scanner->set_border_color(border_color);
                }
                border_cycles++;
                if (border_cycles == 650) {
                    border_cycles = 0;
                }
                if (border_is_hc) {
                    border_color = (vidcycle % 65) & 0x0F;
                    scanner->set_border_color(border_color);
                }
                if (border_is_vc) {
                    border_color = (vidcycle / 65) & 0x0F;
                    scanner->set_border_color(border_color);
                }
       
                scanner->video_cycle();
            
                vidcycle++;
                if (vidcycle == 17030) {
                    vidcycle = 0;
                }
            }
            if (cycle_mode != 4) cycle_mode = 0;
        }
        // now convert scanbuf to frame_byte
        scanbuf = scanner->get_frame_scan();

        start = SDL_GetTicksNS();

        // if we are doing partial frame update (e.g., one cycle, or one scanline) we need to read the texture data back into frame.
        if (cycle_mode != 3 && cycle_mode != 4) {
            SDL_SetRenderTarget(renderer, stage2);
            if (!SDL_RenderTexture(renderer, fr_vsg->get_texture(), nullptr, nullptr)) assert(false);

            SDL_Rect irect = { 0, 0, 910, 263 };
            SDL_Surface *surface = SDL_RenderReadPixels(renderer, &irect);
            SDL_SetRenderTarget(renderer, nullptr);
            fr_vsg->open();
            memcpy(fr_vsg->data(), surface->pixels, 910 * 263 * sizeof(RGBA_t));
            
            if (render_mode == 3) vsg->generate_frame(scanbuf);
            else vsgc->generate_frame(scanbuf);
            
            fr_vsg->close();
            SDL_DestroySurface(surface);
        } else {
            // generate new pixmap and update texture all in one.. or just do a partial update, and, copy updated data back into texture.
            fr_vsg->open();
            if (render_mode == 3) vsg->generate_frame(scanbuf);
            else vsgc->generate_frame(scanbuf);
            fr_vsg->close();
        }

        // Content rectangles for use with VSG2
        constexpr SDL_FRect content_rec_vsg2[3][2] = {
            //{ { 84.0, 12.0, 560+168, 240.0 }, { 0.0, 0.0,0.0,0.0 } }, // no shr here
            //{ { 168.0, 34.0, 560, 192.0 }, { 0.0, 0.0,0.0,0.0 } }, // this is exactly our bounding box.
            
            // II / II+
            { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+21 }, { 0.0, 0.0,0.0,0.0 } },
            
            // IIe - is slightly (7 pixels) wider.
            { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+21 }, { 0.0, 0.0,0.0,0.0 } },
            //{ { 168.0-42-7, 34.0-20, 560+42+42, 192.0+20+20 }, { 0.0, 0.0,0.0,0.0 } },  // use something like this later for composite vsg
            
            // IIgs            
            // { { 0.0, 0.0, 910, 263.0 }, { 0.0, 2.0, 1040, 263.0 } }, // entire content area
            { { 168.0-42, 35.0-19, 560+42+42, 192.0+19+29 }, { 192.0-48.0, 35.0-19.0, 640.0+48+48, 200+19+21.0 } },
        };

        /*
         What we want here is:
         x and y offset
         w and h scale - positive number adds that many pixels on the left and right; negative subtracts. 
        */

        if (frame_moved) {
            int shr_index = (generate_mode >= 7) ? 1 : 0;
            ii_frame_src = content_rec_vsg2[scanner_choice-1][shr_index];
            ii_frame_src.x += (float)hpos-hsize;
            ii_frame_src.y += (float)vpos-vsize;
            ii_frame_src.w += (float)hsize*2;
            ii_frame_src.h += (float)vsize*2;

        }

        // the destination is where the texture is positioned in window.

        // clear backbuffer
        //SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        //if (!SDL_RenderTexture(renderer, stage2, &ii_frame_src, &ii_frame_dst)) {
        if (!SDL_RenderTexture(renderer, fr_vsg->get_texture(), &ii_frame_src, nullptr)) {
            printf("Failed to render stage2 texture: %s\n", SDL_GetError());
        }
        static char txt[100];
        snprintf(txt, 100, "%02dV %02dH", vsg->get_v(), vsg->get_h());
        SDL_RenderDebugText(renderer, 10, 220, txt);

        // Emit!
        SDL_RenderPresent(renderer);      
        end = SDL_GetTicksNS();

        cumulative += (end-start);
        if (framecnt == 300) {
            times[framecnt] = (end-start);
            printf("Render Time taken:%llu  %llu ns per frame\n", u64_t(cumulative), u64_t(cumulative / 300));
            cumulative = 0;
            framecnt = 0;
        }

        while (SDL_GetTicksNS() - frame_start < 16'688'819) ;

    }
    
    printf("Render Time taken:%llu  %llu ns per frame\n", u64_t(cumulative), u64_t(cumulative / 900));
    for (int i = 0; i < (framecnt > 300 ? 300 : framecnt); i++) {
        printf("%llu ", times[i]);
    }
    printf("\n");
    
    return 0;
}
