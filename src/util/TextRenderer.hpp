#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>

enum TextAlignment {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
};

struct TextRenderer {
    /* Display Info */
    SDL_Window *window;
    SDL_Renderer *renderer;

    uint8_t color_r, color_g, color_b, color_a;

    /* TTF Font Info */
    float font_size = 15.0f;
    TTF_Font *font = NULL;
    bool use_debug_font = false;
    TTF_TextEngine *engine = NULL;
    int font_line_height = 14;

    TextRenderer(SDL_Renderer *renderer, const std::string &font_path, float font_size);
    ~TextRenderer();
    void set_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void render(const std::string &text, int x, int y) { render(text, x, y, TEXT_ALIGN_LEFT); }
    void render(const std::string &text, int x, int y, TextAlignment alignment);
    int char_width(char c);
    int string_width(const std::string &text);
    int get_font_line_height() { return font_line_height; }
};

