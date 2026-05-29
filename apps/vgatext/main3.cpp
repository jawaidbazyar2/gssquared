#include "SDL3/SDL_pixels.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <vector>

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

    // Render in a fixed 720x400 coordinate space and let SDL scale it to fill
    // the (resizable) window, preserving aspect ratio.
    SDL_SetRenderLogicalPresentation(renderer, 720, 400, SDL_LOGICAL_PRESENTATION_LETTERBOX);

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

    // Geometry for the whole text page. Every cell is a quad (4 verts / 6
    // indices). Positions and indices are constant, so they're built once here;
    // only the per-vertex UVs are rewritten each frame to reflect the current
    // text page. The entire screen is then drawn in a single SDL_RenderGeometry
    // call instead of 2000 SDL_RenderTexture calls.
    float font_w = 0.0f, font_h = 0.0f;
    SDL_GetTextureSize(font_texture, &font_w, &font_h);

    constexpr int COLS = 80;
    constexpr int ROWS = 25;
    constexpr int NUM_CELLS = COLS * ROWS;
    constexpr float CELL_W = 9.0f;
    constexpr float CELL_H = 16.0f;
    constexpr float ATLAS_COL_STRIDE = 9.0f;
    constexpr float ATLAS_ROW_STRIDE = 17.0f;

    std::vector<SDL_Vertex> vertices(NUM_CELLS * 4);
    std::vector<int> indices(NUM_CELLS * 6);

    const SDL_FColor white = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            int cell = y * COLS + x;
            int v = cell * 4;
            float px = x * CELL_W;
            float py = y * CELL_H;
            vertices[v + 0].position = { px,          py };
            vertices[v + 1].position = { px + CELL_W, py };
            vertices[v + 2].position = { px + CELL_W, py + CELL_H };
            vertices[v + 3].position = { px,          py + CELL_H };
            for (int k = 0; k < 4; k++) vertices[v + k].color = white;

            int idx = cell * 6;
            indices[idx + 0] = v + 0;
            indices[idx + 1] = v + 1;
            indices[idx + 2] = v + 2;
            indices[idx + 3] = v + 0;
            indices[idx + 4] = v + 2;
            indices[idx + 5] = v + 3;
        }
    }

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

        // Rewrite glyph UVs for the current text page contents.
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                int cell = y * COLS + x;
                uint8_t frame_byte = framebuf[cell];

                float sx = (frame_byte & 0x0F) * ATLAS_COL_STRIDE;
                float sy = (frame_byte >> 4)   * ATLAS_ROW_STRIDE;
                float u0 = sx / font_w;
                float v0 = sy / font_h;
                float u1 = (sx + CELL_W) / font_w;
                float v1 = (sy + CELL_H) / font_h;

                int v = cell * 4;
                vertices[v + 0].tex_coord = { u0, v0 };
                vertices[v + 1].tex_coord = { u1, v0 };
                vertices[v + 2].tex_coord = { u1, v1 };
                vertices[v + 3].tex_coord = { u0, v1 };
            }
        }

        // Whole screen in one draw call.
        SDL_RenderGeometry(renderer, font_texture,
                           vertices.data(), (int)vertices.size(),
                           indices.data(), (int)indices.size());
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