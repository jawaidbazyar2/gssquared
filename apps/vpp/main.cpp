#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <SDL3/SDL.h>

//#include "devices/displaypp/frame/frame_bit.hpp"
#include "SDL3/SDL_rect.h"
#include "SDL3/SDL_render.h"
#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/VideoScannerIIe.hpp"
#include "devices/displaypp/VideoScannerIIgs.hpp"
#include "devices/displaypp/VideoScanGenerator.cpp"
#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
#include "devices/displaypp/render/GSRGB560.hpp"
#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/ScanBuffer.hpp"

#include "mmus/mmu_iie.hpp"


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

border_rect_t ii_borders[3][3]; // [y][x]
border_rect_t shr_borders[3][3]; // [y][x]

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

void calculate_border_rects(bool shift_enabled) {
    float shift_offset = shift_enabled ? 7.0f : 0.0f;
    float width = shift_enabled ? 567.0f : 560.0f;

    constexpr float b_l_x = 7.0f;
    constexpr float b_l_w = 6.0f;

    constexpr float b_r_x = 0.0f;
    constexpr float b_r_w = 7.0f;

    // top
    ii_borders[B_TOP][B_LT].src = {b_l_x, 243.0, b_l_w, 19};
    ii_borders[B_TOP][B_LT].dst = {0.0, 0.0, 42.0, 19};

    ii_borders[B_TOP][B_CEN].src = {13, 243.0, 40, 19};
    ii_borders[B_TOP][B_CEN].dst = {42, 0.0, 560, 19};

    ii_borders[B_TOP][B_RT].src = {0, 243.0, b_r_w, 19};
    ii_borders[B_TOP][B_RT].dst = {42.0f+560.0f-shift_offset, 0.0, 56.0, 19};

    // center
    ii_borders[B_CEN][B_LT].src = {b_l_x, 0.0, b_l_w, SCREEN_TEXTURE_HEIGHT};
    ii_borders[B_CEN][B_LT].dst = {0, 19.0, 42.0, SCREEN_TEXTURE_HEIGHT};

    ii_borders[B_CEN][B_CEN].src = {0.0+0.25f, 0.0, width-0.5f, (float)SCREEN_TEXTURE_HEIGHT}; // not from border texture
    ii_borders[B_CEN][B_CEN].dst = {42.0f-shift_offset, 19.0, width, SCREEN_TEXTURE_HEIGHT}; // not from border texture

    ii_borders[B_CEN][B_RT].src = {0.0, 0.0, b_r_w, SCREEN_TEXTURE_HEIGHT};
    ii_borders[B_CEN][B_RT].dst = {42.0f+560.0f-shift_offset, 19.0, 56.0, SCREEN_TEXTURE_HEIGHT};

    // bottom
    ii_borders[B_BOT][B_LT].src = {b_l_x, 192.0, b_l_w, 21};
    ii_borders[B_BOT][B_LT].dst = {0.0, 19+SCREEN_TEXTURE_HEIGHT, 42.0, 21};

    ii_borders[B_BOT][B_CEN].src = {13.0, 192.0, 40, 21};
    ii_borders[B_BOT][B_CEN].dst = {42, 19+SCREEN_TEXTURE_HEIGHT, 560, 21};

    ii_borders[B_BOT][B_RT].src = {0, 192.0, b_r_w, 21};
    ii_borders[B_BOT][B_RT].dst = {42.0f+560.0f-shift_offset, 19+SCREEN_TEXTURE_HEIGHT, 56.0, 21};

    // SHR

    shr_borders[B_CEN][B_LT].src = {0.0, 0.0, b_l_w, SCREEN_TEXTURE_HEIGHT};
    shr_borders[B_CEN][B_LT].dst = {0.0, 19.0, 42.0, 200};

    shr_borders[B_CEN][B_CEN].src = {0.0, 0.0, 640, 200};
    shr_borders[B_CEN][B_CEN].dst = {42.0, 19.0, width, 200};

    shr_borders[B_CEN][B_RT].src = {0.0, 1.0, b_r_w, SCREEN_TEXTURE_HEIGHT};
    shr_borders[B_CEN][B_RT].dst = {42+560, 19.0, 42.0, 200};
}

void print_rect(const char *name, border_rect_t &r) {
    printf("%s: SRC: (%f, %f, %f, %f)\n", name, r.src.x, r.src.y, r.src.w, r.src.h);
    printf("%s: DST: (%f, %f, %f, %f)\n", name, r.dst.x, r.dst.y, r.dst.w, r.dst.h);
}

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
    SDL_ScaleMode scales[3] = { SDL_SCALEMODE_PIXELART, SDL_SCALEMODE_LINEAR, SDL_SCALEMODE_NEAREST };

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

    uint16_t b_w = 184 / 2;
    uint16_t b_h = 40;

    const uint16_t f_w = SCREEN_TEXTURE_WIDTH + b_w, f_h = SCREEN_TEXTURE_HEIGHT + b_h;

    const uint16_t c_w = f_w * 2, c_h = f_h * 4;

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
    /* SDL_Texture *border_texture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 53, 263);
    if (!border_texture) {
        printf("Failed to create texture\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    } */
    /* SDL_Texture *shrtexture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 640, 200);
    if (!shrtexture) {
        printf("Failed to create shrtexture\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    } */
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

    Frame560 *frame_byte = new(std::align_val_t(64)) Frame560(560, II_SCREEN_TEXTURE_HEIGHT);

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

    Frame560RGBA *frame_rgba = new(std::align_val_t(64)) Frame560RGBA(567, II_SCREEN_TEXTURE_HEIGHT, renderer, PIXEL_FORMAT);
    FrameBorder *fr_border = new(std::align_val_t(64)) FrameBorder(53, 263, renderer, PIXEL_FORMAT);
    Frame640 *frame_shr = new(std::align_val_t(64)) Frame640(640, 200, renderer, PIXEL_FORMAT);

    SDL_Texture *rgba_texture = frame_rgba->get_texture();
    SDL_Texture *border_texture = fr_border->get_texture();
    SDL_Texture *shrtexture = frame_shr->get_texture();

    SDL_SetTextureScaleMode(rgba_texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(rgba_texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(border_texture, SDL_SCALEMODE_PIXELART);


    Monochrome560 monochrome;
    NTSC560 ntsc_render;
    GSRGB560 rgb_render;

    uint16_t border_color = 0x0F;

    VideoScannerII *video_scanner_ii = new VideoScannerII(mmu);
    video_scanner_ii->initialize();
    VideoScannerIIe *video_scanner_iie = new VideoScannerIIe(mmu);
    video_scanner_iie->initialize();
    VideoScannerIIgs *video_scanner_iigs = new VideoScannerIIgs(mmu);
    video_scanner_iigs->initialize();
    video_scanner_iigs->set_border_color(0x0F);
    VideoScanGenerator *vsg = new VideoScanGenerator(&iie_rom);

    vsg->set_display_shift(false);
    rgb_render.set_shift_enabled(false);
    ntsc_render.set_shift_enabled(false);
    monochrome.set_shift_enabled(false);
    calculate_border_rects(false);
    print_border_rects();

    int pitch;
    void *pixels;

    uint64_t cumulative = 0;
    uint64_t times[900];
    uint64_t framecnt = 0;

    int render_mode = 1;
    int sharpness = 0;
    bool exiting = false;
    bool flash_state = false;
    int flash_count = 0;
    int scanner_choice = SCANNER_II;
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
                    case SDLK_1:
                        generate_mode = 1;
                        scanner->set_page_1();
                        scanner->reset_80col();
                        scanner->set_text();
                        scanner->reset_shr();
                        break;
                
                    case SDLK_2:
                        generate_mode = 2;
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->set_text();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_3:
                        generate_mode = 3;
                        scanner->set_page_1();
                        scanner->set_graf();
                        scanner->reset_80col();
                        scanner->set_lores();
                        scanner->reset_shr();
                        break;

                    case SDLK_4:
                        generate_mode = 4;
                        scanner->set_graf();
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->set_lores();
                        scanner->set_dblres();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_5:
                        generate_mode = 5;
                        scanner->set_page_2();
                        scanner->reset_80col();
                        scanner->set_graf();
                        scanner->set_hires();
                        scanner->reset_dblres();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_6:
                        generate_mode = 6;
                        scanner->set_dblres();
                        scanner->set_hires();
                        scanner->set_graf();
                        scanner->set_page_1();
                        scanner->set_80col();
                        scanner->reset_shr();
                        break;
                    
                    case SDLK_7:
                        generate_mode = 7;
                        scanner->set_shr();
                        break;
                    
                    case SDLK_8:
                        generate_mode = 8;
                        scanner->set_shr();
                        break;
                    
                    case SDLK_N:
                        render_mode = 2;
                        break;
                    
                    case SDLK_M:
                        render_mode = 1;
                        break;
                    
                    case SDLK_R:
                        render_mode = 3;
                        break;

                    case SDLK_A:
                        scanner->set_altchrset();
                        break;
                    
                    case SDLK_S:
                        scanner->set_altchrset();
                        break;
                    
                    case SDLK_X:
                        scanner->reset_altchrset();
                        break;
                    
                    case SDLK_Z:
                        scanner->set_full();
                        break;
                    
                    case SDLK_P:
                        sharpness = (sharpness + 1) % 3;
                        SDL_SetTextureScaleMode(frame_rgba->get_texture(), scales[sharpness]);
                        SDL_SetTextureScaleMode(shrtexture, scales[sharpness]);
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
                        rgb_render.set_shift_enabled(false);
                        ntsc_render.set_shift_enabled(false);
                        monochrome.set_shift_enabled(false);
                        calculate_border_rects(false);
                        print_border_rects();
                        printf("Scanner choice: II\n");
                        break;
                    
                    case SDLK_F2:
                        scanner_choice = SCANNER_IIE;
                        vsg->set_display_shift(true);
                        rgb_render.set_shift_enabled(true);
                        ntsc_render.set_shift_enabled(true);
                        monochrome.set_shift_enabled(true);
                        calculate_border_rects(true);
                        print_border_rects();
                        printf("Scanner choice: IIe\n");
                        break;
                    
                    case SDLK_F3:
                        scanner_choice = SCANNER_IIGS;
                        vsg->set_display_shift(false);
                        rgb_render.set_shift_enabled(false);
                        ntsc_render.set_shift_enabled(false);
                        monochrome.set_shift_enabled(false);
                        calculate_border_rects(false);
                        print_border_rects();
                        printf("Scanner choice: IIgs\n");
                        break;                
                }
                printf("key pressed: %d\n", event.key.key);
            }
        }

        start = SDL_GetTicksNS();
        int phaseoffset = 1; // now that I start normal (40) display at pixel 7, its phase is 1 also. So, both 40 and 80 display start at phase 1 now.
        ScanBuffer *frame_scan = nullptr;

        /* exactly one frame worth of video cycles */
        for (int vidcycle = 0; vidcycle < 17030; vidcycle++) {
            // hard-code a "cycle timed video switch" splitting screen into half hires and half lores
            /* if (vidcycle < 17030/2) {
                video_scanner_iie->set_hires();
                video_scanner_iie->set_page_2();
            } else {
                video_scanner_iie->set_lores();
                video_scanner_iie->set_page_1();
            }
            if (vidcycle % 5 < 2) { // every 5 cycles will create columns on the screen of different video modes.
                video_scanner_iie->set_hires();
                video_scanner_iie->set_page_2();
            } else {
                video_scanner_iie->set_lores();
                video_scanner_iie->set_page_1();
            }  */
            if ((rolling_border) && (++border_cycles == 650)) {
                border_color = (border_color + 1) & 0x0F;
                fg = (fg + 1) & 0x0F;
                bg = (bg + 1) & 0x0F;
                scanner->set_text_fg(fg);
                scanner->set_text_bg(bg);
                scanner->set_border_color(border_color);

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
        }
        // now convert frame_scan to frame_byte
        frame_scan = scanner->get_frame_scan();

        if ((scanner_choice == SCANNER_II) || (scanner_choice == SCANNER_IIE)) {
            if (generate_mode < 7) {
                vsg->generate_frame(
                    frame_scan, 
                    frame_byte, 
                    nullptr,
                    nullptr
                );
                //frame_byte->print();
                frame_rgba->open();
                switch (render_mode) {
                    case 1:
                        monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
                        break;
                    case 2:
                        ntsc_render.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF) ); // no-color color is white.
                        break;
                    case 3:
                        rgb_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF)  );
                        break;
                }
                frame_rgba->close();
            }
        } else {
            fr_border->open();
            frame_shr->open();
            vsg->generate_frame(
                frame_scan, 
                frame_byte, 
                fr_border,
                frame_shr
            );
            if (generate_mode < 7) {
                frame_rgba->open();
                switch (render_mode) {
                    case 1:
                        monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
                        break;
                    case 2:
                        ntsc_render.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF) ); // no-color color is white.
                        break;
                    case 3:
                        rgb_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF)  );
                        break;
                }
                frame_rgba->close();
            }

            frame_shr->close();
            fr_border->close();

        }
    
#if 0
        if (generate_mode < 7) {
            if (scanner_choice == SCANNER_IIGS) {
                fr_border->open();
                frame_shr->open();
            }
            vsg->generate_frame(
                frame_scan, 
                frame_byte, 
                (scanner_choice == SCANNER_IIGS) ? fr_border : nullptr,
                (scanner_choice == SCANNER_IIGS) ? frame_shr : nullptr
            );
            if (scanner_choice == SCANNER_IIGS) {
                fr_border->close();
                frame_shr->close();
            }
            frame_rgba->open();
            switch (render_mode) {
                case 1:
                    monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
                    break;
                case 2:
                    ntsc_render.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF) ); // no-color color is white.
                    break;
                case 3:
                    rgb_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF)  );
                    break;
            }
            frame_rgba->close();

            // update the legacy II texture
            /* SDL_LockTexture(texture, NULL, &pixels, &pitch);
            memcpy(pixels, frame_rgba->data(), II_SCREEN_TEXTURE_WIDTH * II_SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
            SDL_UnlockTexture(texture); */
        } else {
            frame_rgba->open();
            frame_shr->open();
            fr_border->open();
            vsg->generate_frame(
                frame_scan, 
                frame_byte, 
                (scanner_choice == SCANNER_IIGS) ? fr_border : nullptr,
                (scanner_choice == SCANNER_IIGS) ? frame_shr : nullptr
            );
            frame_shr->close();
            fr_border->close();
            frame_rgba->close();
            // update the shr texture
            /* SDL_LockTexture(shrtexture, NULL, &pixels, &pitch);
            memcpy(pixels, frame_shr->data(), 640 * 200 * sizeof(RGBA_t));
            SDL_UnlockTexture(shrtexture); */
        }

        /* if (old_scanner_choice == SCANNER_IIGS) {
            SDL_LockTexture(border_texture, NULL, &pixels, &pitch);
            memcpy(pixels, fr_border->data(), 53 * 263 * sizeof(RGBA_t));
            SDL_UnlockTexture(border_texture);
        } */
#endif
        // clear backbuffer
        SDL_RenderClear(renderer);

        // draw some border
        if (scanner_choice == SCANNER_IIGS) {
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_TOP][B_LT].src, &ii_borders[B_TOP][B_LT].dst); // top left
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_TOP][B_CEN].src, &ii_borders[B_TOP][B_CEN].dst); // top
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_TOP][B_RT].src, &ii_borders[B_TOP][B_RT].dst); // top right

            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_CEN][B_LT].src, &ii_borders[B_CEN][B_LT].dst); // left
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_CEN][B_RT].src, &ii_borders[B_CEN][B_RT].dst); // right
        
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_BOT][B_LT].src, &ii_borders[B_BOT][B_LT].dst); // bottom left
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_BOT][B_CEN].src, &ii_borders[B_BOT][B_CEN].dst); // bottom
            SDL_RenderTexture(renderer, border_texture, &ii_borders[B_BOT][B_RT].src, &ii_borders[B_BOT][B_RT].dst); // bottom right
        }

        // Draw the screen texture.
        if (generate_mode < 7) {
            // draw over border but shiftable portions need to be alpha'd with border color.
            SDL_RenderTexture(renderer, frame_rgba->get_texture(), &ii_borders[B_CEN][B_CEN].src, &ii_borders[B_CEN][B_CEN].dst); 
        } else {
            SDL_RenderTexture(renderer, shrtexture, &shr_borders[B_CEN][B_CEN].src, &shr_borders[B_CEN][B_CEN].dst);
        }

        // Emit!
        SDL_RenderPresent(renderer);      
        end = SDL_GetTicksNS();

        cumulative += (end-start);
        if (framecnt == 300) {
            times[framecnt] = (end-start);
            printf("Render Time taken:%llu  %llu ns per frame\n", cumulative, cumulative / 300);
            cumulative = 0;
            framecnt = 0;
        }

        while (SDL_GetTicksNS() - frame_start < 16'688'819) ;

    }
    
    printf("Render Time taken:%llu  %llu ns per frame\n", cumulative, cumulative / 900);
    for (int i = 0; i < (framecnt > 300 ? 300 : framecnt); i++) {
        printf("%llu ", times[i]);
    }
    printf("\n");
    
    return 0;
}
