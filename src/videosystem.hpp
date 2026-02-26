#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <map>
#include "computer.hpp"
#include "util/EventQueue.hpp"
#include "display/types.hpp"
#include "ui/Clipboard.hpp"
#include "devices/displaypp/RGBA.hpp"

// somewhere calculate the window size properly (42+49, and 19+21)
#define BORDER_WIDTH 42
#define BORDER_HEIGHT 20

typedef enum {
    DISPLAY_WINDOWED_MODE = 0,
    DISPLAY_FULLSCREEN_MODE = 1,
    NUM_FULLSCREEN_MODES
} display_fullscreen_mode_t;

/** Display Modes */

typedef enum {
    DM_ENGINE_NTSC = 0,
    DM_ENGINE_RGB,
    DM_ENGINE_MONO,
    DM_NUM_COLOR_ENGINES
} display_color_engine_t;

typedef enum {
    DM_MONO_WHITE = 0,
    DM_MONO_GREEN,
    DM_MONO_AMBER,
    DM_NUM_MONO_MODES
} display_mono_color_t;

typedef enum {
    DM_PIXEL_FUZZ = 0,
    DM_PIXEL_SQUARE,
    DM_NUM_PIXEL_MODES
} display_pixel_mode_t;


struct video_system_t {
    using FrameHandler = std::function<bool(bool)>;

    std::multimap<int, FrameHandler, std::greater<int>> frame_handlers;

    SDL_Window *window; // primary emulated display window
    SDL_Renderer* renderer;

    display_fullscreen_mode_t display_fullscreen_mode = DISPLAY_WINDOWED_MODE;
    display_color_engine_t display_color_engine = DM_ENGINE_NTSC;
    display_mono_color_t display_mono_color = DM_MONO_GREEN;
    display_pixel_mode_t display_pixel_mode = DM_PIXEL_FUZZ;

    int border_width = BORDER_WIDTH;
    int border_height = BORDER_HEIGHT;
    float aspect_ratio = 0.0;
    float scale_x = 2.0f;
    float scale_y = 4.0f;
    int window_width = 0;
    int window_height = 0;

    float fullscreen_x_shift = 0.0f;

    EventQueue *event_queue = nullptr;

    bool force_full_frame_redraw = false;

    ClipboardImage *clip = nullptr;

    SDL_Texture *last_texture = nullptr;
    SDL_FRect last_srcrect = { 0.0f, 0.0f, 0.0f, 0.0f };

    RGBA_t mono_color_table[DM_NUM_MONO_MODES] = {
        {.a=0xFF, .b=0xFF, .g=0xFF, .r=0xFF }, // white
        {.a=0xFF, .b=0x4A, .g=0xFF, .r=0x00 }, // green (was 55) chosen from measuring @ 549nm and https://academo.org/demos/wavelength-to-colour-relationship/
        {.a=0xFF, .b=0x00, .g=0xBF, .r=0xFF }  // amber
    };
   /*  uint32_t mono_color_table_u[DM_NUM_MONO_MODES] = { // NO LONGER USED
        0xFFFFFFFF, // white
        0x00FF4AFF, // green (was 55) chosen from measuring @ 549nm and https://academo.org/demos/wavelength-to-colour-relationship/
        0xFFBF00FF, // amber
    }; */
    video_system_t(computer_t *computer);
    ~video_system_t();
    void set_window_title(const char *title);
    void window_resize(const SDL_Event &event);
    void toggle_fullscreen();
    void set_window_fullscreen(display_fullscreen_mode_t mode);
    display_fullscreen_mode_t get_window_fullscreen();
    void sync_window();
    void render_frame(SDL_Texture *texture, SDL_FRect *srcrect, SDL_FRect *dstrect , bool respect_mode = true  /* , float offset */);
    void render_frame_simple(SDL_Texture *texture, SDL_FRect *srcrect);
    void clear();
    void present();
    bool display_capture_mouse(bool capture);
    bool is_mouse_captured();
    void raise();
    void raise(SDL_Window *window);
    void hide(SDL_Window *window);
    void show(SDL_Window *window);
    void set_full_frame_redraw();
    void send_engine_message();
    void toggle_display_engine();
    void set_display_engine(display_color_engine_t mode);
    void set_display_mono_color(display_mono_color_t mode);
    void flip_display_scale_mode();
    void register_frame_processor(int weight, FrameHandler handler);
    void update_display(bool force_full_frame = false);
    RGBA_t get_mono_color() { return mono_color_table[display_mono_color]; };
};
