#include "SDL3/SDL_pixels.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>

int main(int argc, char *argv[]) {

    uint8_t framebuf[2048];
    uint8_t attrbuf[2048];

    for (int i = 0; i < 2048; i++) {
        framebuf[i] = i & 0xFF;
        attrbuf[i] = (i+1) & 0xFF;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("vgatext",720, 400, SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowAspectRatio(window, 1.8f, 1.8f);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        printf("Renderer could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // frame texture
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 720, 400);
    if (texture == NULL) {
        printf("Texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }

    // load font bitmap
    SDL_Surface *font_surface = IMG_Load("apps/vgatext/IBM_VGA_8x16.png");
    if (font_surface == NULL) {
        printf("Font bitmap could not be loaded! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Texture *font_texture = SDL_CreateTextureFromSurface(renderer, font_surface);
    if (font_texture == NULL) {
        printf("Font texture could not be created! SDL_Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_DestroySurface(font_surface);

    uint64_t framestats[300];
    int framecount = 0;
    SDL_Event event;
    while (1) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                return 0;
            }
        }
        uint64_t start = SDL_GetTicksNS();

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // update texture 
        SDL_SetRenderTarget(renderer, texture);
        for (int y = 0; y < 25; y++) {
            for (int x = 0; x < 80; x++) {
                int addr = y * 80 + x;
                uint8_t frame_byte = framebuf[addr];
                uint8_t attr_byte = attrbuf[addr];
                
                //SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 255);
                SDL_FRect dstrect = { x * 9.0f, y * 16.0f, 9.0f, 16.0f };
                SDL_FRect srcrect = { 
                    (frame_byte & 0xF) * 9.0f, 
                    (frame_byte >> 4) * 17.0f, 
                    9.0f, 16.0f };
                SDL_RenderTexture(renderer, font_texture, &srcrect, &dstrect);
            }
        }
        SDL_SetRenderTarget(renderer, nullptr);
        static SDL_FRect srcrect = { 0.0f, 0.0f, 720.0f, 400.0f };
        SDL_RenderTexture(renderer, texture, &srcrect, NULL);
        uint64_t end = SDL_GetTicksNS();

        SDL_RenderPresent(renderer);
        
        // 300 frames = 5 seconds
        framestats[framecount] = end - start;
        framecount = (framecount + 1) % 300;
        if (framecount == 0) {
            uint64_t frametotal = 0;
            for (int i = 0; i < 300; i++) {
                frametotal += framestats[i];
            }
            printf("Average frame time: %llu ns\n", frametotal / 300);
        }
        SDL_Delay(16);
//        SDL_DelayNS(16'688'819 - (end - start));
    }

    return 0;
}