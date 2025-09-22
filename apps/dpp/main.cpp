#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

#include <SDL3/SDL.h>

//#include "devices/displaypp/frame/frame_bit.hpp"
#include "devices/displaypp/frame/Frames.hpp"
#include "devices/displaypp/generate/AppleII.cpp"
#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
#include "devices/displaypp/render/GSRGB560.hpp"
#include "devices/displaypp/CharRom.hpp"

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

#define CANVAS_WIDTH ((560+20)*2)
#define CANVAS_HEIGHT (192*4)
#define SCREEN_TEXTURE_WIDTH (560+20)
#define SCREEN_TEXTURE_HEIGHT (192)

int main(int argc, char **argv) {
    uint64_t start = 0, end = 0;

    //SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("DisplayPP Test Harness", CANVAS_WIDTH, CANVAS_HEIGHT, SDL_WINDOW_RESIZABLE);
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
    if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED)) {
        printf("Failed to set render vsync\n");
        printf("SDL Error: %s\n", SDL_GetError());
        return 1;
    }

    const char *rname = SDL_GetRendererName(renderer);
    printf("Renderer: %s\n", rname);
    //SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    SDL_SetRenderScale(renderer, 2.0f, 4.0f);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE); 
    int error = SDL_SetRenderTarget(renderer, nullptr);

    int testiterations = 10000;

#if 0
    // bitstream test. This performs worse than the bytestream.
    printf("Testing bitstream\n");

    start = SDL_GetTicksNS();
    alignas(64) Frame_Bitstream frame(640, 200);
   
    for (int numframes = 0; numframes < testiterations; numframes++) {
        frame.clear();
        for (int i = 0; i < FB_BITSTREAM_HEIGHT; i++) {
            frame.write_line(i);
            for (int j = 0; j < FB_BITSTREAM_WIDTH/2; j++) {
                frame.push(1);
                frame.push(0);
            }
            frame.close_line();
        }
    }

    end = SDL_GetTicksNS();
    frame.print();
    printf("Write Time taken: %llu microseconds per frame\n", (end - start) / 1000 / testiterations);

    start = SDL_GetTicksNS();

    for (int numframes = 0; numframes < testiterations; numframes++) {
        for (int i = 0; i < FB_BITSTREAM_HEIGHT; i++) {
            frame.read_line(i);
            for (int i = 0; i < FB_BITSTREAM_WIDTH/2; i++) {
                frame.pull();
                frame.pull();
            }
        }
    }
    end = SDL_GetTicksNS();
    printf("read Time taken: %llu microseconds per frame\n", (end - start) / 1000 / testiterations);

    printf("Testing bytestream\n");
#endif

    const uint16_t f_w = SCREEN_TEXTURE_WIDTH, f_h = SCREEN_TEXTURE_HEIGHT;
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

    uint8_t text_page[1024];
    uint8_t alt_text_page[1024];
    for (int i = 0; i < 1024; i++) {
        text_page[i] = i & 0xFF;
        alt_text_page[i] = (i+1) & 0xFF;
    }

    uint8_t *lores_page = new uint8_t[1024];
    uint8_t *alt_lores_page = new uint8_t[1024];
    generate_dlgr_test_pattern(lores_page, alt_lores_page);

    //const char *testhgrpic_path = "/Users/bazyar/src/hgrdecode/HIRES/APPLE";
    const char *testhgrpic_path = "/Users/bazyar/src/gssquared/dump.hgr";
    uint8_t *testhgrpic = new(std::align_val_t(64)) uint8_t[8192];
    FILE *f = fopen(testhgrpic_path, "rb");
    if (!f) {
        printf("Failed to load testhgrpic: %s\n", testhgrpic_path);
        return 1;
    }
    fread(testhgrpic, 1, 8192, f);
    fclose(f);

    const char *testdhgrpic_path = "/Users/bazyar/src/hgrdecode/DHIRES/LOGO.DHGR";
    uint8_t *testdhgrpic = new(std::align_val_t(64)) uint8_t[16386];
    FILE *f2 = fopen(testdhgrpic_path, "rb");
    if (!f2) {
        printf("Failed to load testdhgrpic: %s\n", testdhgrpic_path);
        return 1;
    }
    fread(testdhgrpic, 1, 16384, f2);
    fclose(f2);

    CharRom iiplus_rom("resources/roms/apple2_plus/char.rom");
    CharRom iie_rom("resources/roms/apple2e_enh/char.rom");

    if (!iiplus_rom.is_valid() || !iie_rom.is_valid()) {
        printf("Failed to load char roms\n");
        return 1;
    }

    Frame560RGBA *frame_rgba = new(std::align_val_t(64)) Frame560RGBA(f_w, f_h);

    Monochrome560 monochrome;
    NTSC560 ntsc_render;
    GSRGB560 rgb_render;

    AppleII_Display display_iie(&iie_rom);
    iie_rom.print_matrix(0x40);
    AppleII_Display display_iiplus(&iiplus_rom);
    iiplus_rom.print_matrix(0x40);
    //display.set_char_set(true);

    start = SDL_GetTicksNS();
    for (int numframes = 0; numframes < testiterations; numframes++) {
        for (int l = 0; l < 24; l++) {
            display_iiplus.generate_text40(text_page, frame_byte, l);
        }
        monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
    }

    end = SDL_GetTicksNS();
    printf("text Time taken: %llu ns per frame\n", (end - start) / testiterations);

    SDL_FRect dstrect = {
        (float)0.0,
        (float)0.0,
        (float)SCREEN_TEXTURE_WIDTH, 
        (float)SCREEN_TEXTURE_HEIGHT
    };
    SDL_FRect srcrect = {
        (float)0.0,
        (float)0.0,
        (float)SCREEN_TEXTURE_WIDTH, 
        (float)SCREEN_TEXTURE_HEIGHT
    };

    int pitch;
    void *pixels;

    SDL_LockTexture(texture, NULL, &pixels, &pitch);
    std::memcpy(pixels, frame_rgba->data(), SCREEN_TEXTURE_WIDTH * SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
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

    while (++framecnt && !exiting)  {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                exiting = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_1) {
                    generate_mode = 1;
                }
                if (event.key.key == SDLK_2) {
                    generate_mode = 2;
                }
                if (event.key.key == SDLK_3) {
                    generate_mode = 3;
                }
                if (event.key.key == SDLK_4) {
                    generate_mode = 4;
                }
                if (event.key.key == SDLK_5) {
                    generate_mode = 5;
                }
                if (event.key.key == SDLK_6) {
                    generate_mode = 6;
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
                if (event.key.key == SDLK_P) {
                    sharpness = 1 - sharpness;
                    SDL_SetTextureScaleMode(texture, sharpness ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
                }
            }
        }

        start = SDL_GetTicksNS();
        int phaseoffset = 1; // now that I start normal (40) display at pixel 7, its phase is 1 also. So, both 40 and 80 display start at phase 1 now.

        if (flash_count++ > 14) {
            flash_state = !flash_state;
            flash_count = 0;
            display_iiplus.set_flash_state(flash_state);
        }

        for (int l = 0; l < 24; l++) {
            switch (generate_mode) {
                case 1:
                    display_iiplus.generate_text40(text_page, frame_byte, l);
                    break;
                case 2:
                    display_iie.generate_text80(text_page, alt_text_page, frame_byte, l);
                    phaseoffset = 1;
                    break;
                case 3:
                    display_iie.generate_lores40(text_page, frame_byte, l);
                    break;
                case 4:
                    display_iie.generate_lores80(lores_page, alt_lores_page, frame_byte, l);
                    phaseoffset = 1;
                    break;        
                case 5:
                    display_iiplus.generate_hires40(testhgrpic, frame_byte, l);
                    break;
                case 6:
                    // saved dhgr files are aux memory first, then main memory.
                    display_iiplus.generate_hires80(testdhgrpic+0x2000, testdhgrpic, frame_byte, l);
                    phaseoffset = 1;
                    break;
            }
        }
        switch (render_mode) {
            case 1:
                monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF));
                break;
            case 2:
                ntsc_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF), phaseoffset);
                break;
            case 3:
                if (generate_mode == 1 || generate_mode == 2) monochrome.render(frame_byte, frame_rgba, RGBA_t::make(0xFF, 0xFF, 0xFF, 0xFF));
                else rgb_render.render(frame_byte, frame_rgba, RGBA_t::make(0x00, 0xFF, 0x00, 0xFF), phaseoffset);
                break;
        }

        // update the texture - approx 300us
        SDL_LockTexture(texture, NULL, &pixels, &pitch);
        std::memcpy(pixels, frame_rgba->data(), SCREEN_TEXTURE_WIDTH * SCREEN_TEXTURE_HEIGHT * sizeof(RGBA_t));
        SDL_UnlockTexture(texture);
        
        // update widnow - approx 300us
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, &srcrect, &dstrect);       
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
