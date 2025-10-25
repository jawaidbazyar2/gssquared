#include <__new/global_new_delete.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include <SDL3/SDL.h>

//#include "devices/displaypp/frame/frame_bit.hpp"
#include "SDL3/SDL_rect.h"
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

//#define SCREEN_TEXTURE_WIDTH (560+20)
#define SCREEN_TEXTURE_WIDTH (567)
//#define SCREEN_TEXTURE_HEIGHT (192)
#define SCREEN_TEXTURE_HEIGHT (192)

#define SCANNER_II 1
#define SCANNER_IIE 2
#define SCANNER_IIGS 3

struct border_rect_t {
    SDL_FRect src;
    SDL_FRect dst;
};

border_rect_t borders[3][3]; // [y][x]

#define B_TOP 0
#define B_CEN 1
#define B_BOT 2
#define B_LT 0
#define B_RT 2

void init_border_rects() {
    borders[B_CEN][B_LT].src = {0.0, 0.0, 6.0, SCREEN_TEXTURE_HEIGHT};
    borders[B_CEN][B_LT].dst = {0.0, 19.0, 42.0, SCREEN_TEXTURE_HEIGHT};

    borders[B_CEN][B_CEN].src = {0.0, 0.0, SCREEN_TEXTURE_WIDTH, (float)SCREEN_TEXTURE_HEIGHT};
    borders[B_CEN][B_CEN].dst = {42.0-7.0, 19.0, SCREEN_TEXTURE_WIDTH, SCREEN_TEXTURE_HEIGHT};

    borders[B_CEN][B_RT].src = {6.0, 0.0, 6.0, SCREEN_TEXTURE_HEIGHT};
    borders[B_CEN][B_RT].dst = {42+560, 19.0, 42.0, SCREEN_TEXTURE_HEIGHT};
}

int main(int argc, char **argv) {
    init_border_rects();

    uint64_t start = 0, end = 0;
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
    SDL_Texture *texture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, SCREEN_TEXTURE_WIDTH, SCREEN_TEXTURE_HEIGHT);
    if (!texture) {
        printf("Failed to create texture\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Texture *border_texture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_STREAMING, 13, SCREEN_TEXTURE_HEIGHT);
    if (!border_texture) {
        printf("Failed to create texture\n");
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
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE); 
    int error = SDL_SetRenderTarget(renderer, nullptr);

    int testiterations = 10000;

    Frame560 *frame_byte = new(std::align_val_t(64)) Frame560(f_w, f_h);

    start = SDL_GetTicksNS();
    for (int numframes = 0; numframes < testiterations; numframes++) {
        for (int i = 0; i < 192; i++) {
            frame_byte->set_line(i);
            for (int j = 0; j < 560/2; j++) {
                frame_byte->push(1);
                frame_byte->push(0);
            }
        }
    }
    end = SDL_GetTicksNS();
    printf("Write Time taken: %llu ns per frame\n", (end - start) / testiterations);

    start = SDL_GetTicksNS();
    int c = 0;
    for (int numframes = 0; numframes < testiterations; numframes++) {
        for (int i = 0; i < f_h; i++) {
            frame_byte->set_line(i);
            for (int j = 0; j < f_w/2; j++) {
                c += frame_byte->pull();
                c += frame_byte->pull();
            }
        }
    }
    end = SDL_GetTicksNS();
    printf("read Time taken: %llu ns per frame\n", (end - start) / testiterations);
    //printf("Size of bytestream entries: %zu bytes\n", sizeof(bs_t));
    printf("c: %d\n", c);
    //frame_byte->print();

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

    const char *testhgrpic_path = "/Users/bazyar/src/hgrdecode/HIRES/APPLE";
    //const char *testhgrpic_path = "/Users/bazyar/src/gssquared/dump.hgr";
//    uint8_t *testhgrpic = new(std::align_val_t(64)) uint8_t[8192];
    uint8_t *testhgrpic = ram + 0x04000;
    FILE *f = fopen(testhgrpic_path, "rb");
    if (!f) {
        printf("Failed to load testhgrpic: %s\n", testhgrpic_path);
        return 1;
    }
    fread(testhgrpic, 1, 8192, f);
    fclose(f);

    const char *testdhgrpic_path = "/Users/bazyar/src/hgrdecode/DHIRES/LOGO.DHGR";
    uint8_t *testdhgrpic_alt = ram + 0x12000;
    uint8_t *testdhgrpic = ram + 0x02000;
//    uint8_t *testdhgrpic = new(std::align_val_t(64)) uint8_t[16386];
    FILE *f2 = fopen(testdhgrpic_path, "rb");
    if (!f2) {
        printf("Failed to load testdhgrpic: %s\n", testdhgrpic_path);
        return 1;
    }
    fread(testdhgrpic_alt, 1, 8192, f2);
    fread(testdhgrpic, 1, 8192, f2);
    fclose(f2);

    CharRom iiplus_rom("resources/roms/apple2_plus/char.rom");
    CharRom iie_rom("resources/roms/apple2e_enh/char.rom");

    if (!iiplus_rom.is_valid() || !iie_rom.is_valid()) {
        printf("Failed to load char roms\n");
        return 1;
    }

    Frame560RGBA *frame_rgba = new(std::align_val_t(64)) Frame560RGBA(f_w, f_h);
    FrameBorder *fr_border = new(std::align_val_t(64)) FrameBorder(13, 262);

    Monochrome560 monochrome;
    NTSC560 ntsc_render;
    GSRGB560 rgb_render;

    uint16_t border_color = 0x0F;

    VideoScannerII *video_scanner_ii = new VideoScannerII(mmu);
    VideoScannerIIe *video_scanner_iie = new VideoScannerIIe(mmu);
    VideoScannerIIgs *video_scanner_iigs = new VideoScannerIIgs(mmu);
    video_scanner_iigs->set_border_color(0x0F);
    VideoScanGenerator *vsg = new VideoScanGenerator(&iie_rom);

/*     AppleII_Display display_iie(iie_rom);
    iie_rom.print_matrix(0x40);
    AppleII_Display display_iiplus(iiplus_rom);
    iiplus_rom.print_matrix(0x40); */

    start = SDL_GetTicksNS();
/*     for (int numframes = 0; numframes < testiterations; numframes++) {
        for (int l = 0; l < 24; l++) {
            display_iiplus.generate_text40(text_page, frame_byte, l);
        }
        monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
    }
 */
    end = SDL_GetTicksNS();
    printf("text Time taken: %llu ns per frame\n", (end - start) / testiterations);

    int pitch;
    void *pixels;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    memcpy(pixels, frame_rgba->data(), SCREEN_TEXTURE_WIDTH * SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
    SDL_UnlockTexture(texture);

    uint64_t cumulative = 0;
    uint64_t times[900];
    uint64_t framecnt = 0;

    int generate_mode = 1;
    int render_mode = 1;
    int sharpness = 0;
    bool exiting = false;
    bool flash_state = false;
    int flash_count = 0;
    int scanner_choice = SCANNER_IIE;
    int old_scanner_choice = -1;
    bool rolling_border = false;

    while (++framecnt && !exiting)  {
        VideoScannerII *scanner;
        
        if (old_scanner_choice != scanner_choice) {
            if (scanner_choice == SCANNER_II) scanner = video_scanner_ii;
            else if (scanner_choice == SCANNER_IIE) scanner = video_scanner_iie;
            else if (scanner_choice == SCANNER_IIGS) scanner = video_scanner_iigs;
            old_scanner_choice = scanner_choice;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exiting = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_1) {
                    generate_mode = 1;
                    scanner->set_page_1();
                    scanner->reset_80col();
                    scanner->set_text();
                }
                if (event.key.key == SDLK_2) {
                    generate_mode = 2;
                    scanner->set_page_1();
                    scanner->set_80col();
                    scanner->set_text();
                }
                if (event.key.key == SDLK_3) {
                    generate_mode = 3;
                    scanner->set_page_1();
                    scanner->set_graf();
                    scanner->reset_80col();
                    scanner->set_lores();
                }
                if (event.key.key == SDLK_4) {
                    generate_mode = 4;
                    scanner->set_graf();
                    scanner->set_page_1();
                    scanner->set_80col();
                    scanner->set_lores();
                    scanner->set_dblres();
                }
                if (event.key.key == SDLK_5) {
                    generate_mode = 5;
                    scanner->set_page_2();
                    scanner->reset_80col();
                    scanner->set_graf();
                    scanner->set_hires();
                    scanner->reset_dblres();
                }
                if (event.key.key == SDLK_6) {
                    generate_mode = 6;
                    scanner->set_dblres();
                    scanner->set_hires();
                    scanner->set_graf();
                    scanner->set_page_1();
                    scanner->set_80col();
                    //video_scanner_iie->set_hires();
                }
                if (event.key.key == SDLK_N) {
                    render_mode = 2;
                }
                if (event.key.key == SDLK_M) {
                    render_mode = 1;
                }
                if (event.key.key == SDLK_R) {
                    render_mode = 3;
                }
                if (event.key.key == SDLK_A) {
                    scanner->set_altchrset();
                }
                if (event.key.key == SDLK_S) {
                    scanner->reset_altchrset();
                }
                if (event.key.key == SDLK_X) {
                    scanner->set_mixed();
                }
                if (event.key.key == SDLK_Z) {
                    scanner->set_full();
                }
                if (event.key.key == SDLK_P) {
                    sharpness = 1 - sharpness;
                    SDL_SetTextureScaleMode(texture, sharpness ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                }
                if (event.key.key == SDLK_B) {
                    border_color = (border_color + 1) & 0x0F;
                    scanner->set_border_color(border_color);
                }
                if (event.key.key == SDLK_V) {
                    rolling_border = !rolling_border;
                }
                if (event.key.key == SDLK_F1) {
                    scanner_choice = SCANNER_II;
                    printf("Scanner choice: II\n");
                }
                if (event.key.key == SDLK_F2) {
                    scanner_choice = SCANNER_IIE;
                    printf("Scanner choice: IIe\n");
                }
                if (event.key.key == SDLK_F3) {
                    scanner_choice = SCANNER_IIGS;
                    printf("Scanner choice: IIgs\n");
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
            if (rolling_border) {
                border_color = (border_color + 1) & 0x0F;
                scanner->set_border_color(border_color);
            }
    
            scanner->video_cycle();
        }
        // now convert frame_scan to frame_byte
        frame_scan = scanner->get_frame_scan();
        vsg->generate_frame(frame_scan, frame_byte, (old_scanner_choice == SCANNER_IIGS) ? fr_border : nullptr);
        
        uint16_t cnt;
        if ((cnt = frame_scan->get_count()) != 0) {
            printf("Frame scan count: %u\n", cnt);
        }
        switch (render_mode) {
            case 1:
                monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
                break;
            case 2:
                ntsc_render.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF), phaseoffset); // no-color color is white.
                break;
            case 3:
                if (generate_mode == 1 || generate_mode == 2) monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF));
                else rgb_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF), phaseoffset);
                break;
        }

        // update the texture
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        memcpy(pixels, frame_rgba->data(), SCREEN_TEXTURE_WIDTH * SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
        SDL_UnlockTexture(texture);

        if (old_scanner_choice == SCANNER_IIGS) {
            SDL_LockTexture(border_texture, NULL, &pixels, &pitch);
            memcpy(pixels, fr_border->data(), 13 * SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
            SDL_UnlockTexture(border_texture);
        }

        // update widnow
        SDL_RenderClear(renderer);
        if (old_scanner_choice == SCANNER_IIGS) {
            SDL_RenderTexture(renderer, border_texture, &borders[B_CEN][B_LT].src, &borders[B_CEN][B_LT].dst);
        }
        // draw over border but shiftable portions need to be alpha'd with border color.
        SDL_RenderTexture(renderer, texture, &borders[B_CEN][B_CEN].src, &borders[B_CEN][B_CEN].dst); 
        if (old_scanner_choice == SCANNER_IIGS) {
            SDL_RenderTexture(renderer, border_texture, &borders[B_CEN][B_RT].src, &borders[B_CEN][B_RT].dst);
        }

        end = SDL_GetTicksNS();
        SDL_RenderPresent(renderer);      

        cumulative += (end-start);
        if (framecnt == 300) {
            times[framecnt] = (end-start);
            printf("Render Time taken:%llu  %llu ns per frame\n", cumulative, cumulative / 300);
            cumulative = 0;
            framecnt = 0;
        }
    }
    
    printf("Render Time taken:%llu  %llu ns per frame\n", cumulative, cumulative / 900);
    for (int i = 0; i < (framecnt > 300 ? 300 : framecnt); i++) {
        printf("%llu ", times[i]);
    }
    printf("\n");
    
    return 0;
}
