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

    SDL_Window *window = nullptr; // primary emulated display window
    SDL_Renderer* renderer = nullptr;
    SDL_Surface *headless_surface = nullptr; // offscreen target when headless (no window)
    // Non-null when the renderer is the SDL GPU-backed renderer (required for
    // custom fragment-shader post-processing). Null means we fell back to the
    // classic renderer and shader effects are unavailable.
    SDL_GPUDevice *gpu_device = nullptr;
    // CRT post-process fragment shader and its render state. Only created when
    // gpu_device is non-null. Null when unavailable.
    SDL_GPUShader *crt_shader = nullptr;
    SDL_GPURenderState *crt_state = nullptr;
    // Offscreen render target the emulator frame is drawn into when the CRT
    // shader is active; it is then blitted to the swapchain through the shader.
    // Sized to the renderer's pixel output and recreated on resize.
    SDL_Texture *scene_target = nullptr;
    int scene_target_w = 0;
    int scene_target_h = 0;
    // User toggle for the CRT post-process shader. Only takes effect when the
    // GPU renderer + shader are available (gpu_device and crt_state non-null).
    bool crt_shader_enabled = false;
    SDL_Texture *screencap_texture = nullptr;
    
    display_fullscreen_mode_t display_fullscreen_mode = DISPLAY_WINDOWED_MODE;
    display_color_engine_t display_color_engine = DM_ENGINE_NTSC;
    display_mono_color_t display_mono_color = DM_MONO_GREEN;
    display_pixel_mode_t display_pixel_mode = DM_PIXEL_FUZZ;

    SDL_FRect target = { 0.0f, 0.0f, 0.0f, 0.0f };

    int border_width = BORDER_WIDTH;
    int border_height = BORDER_HEIGHT;
    float aspect_ratio = 0.0;
    /* float scale_x = 2.0f;
    float scale_y = 4.0f; */
    int window_width = 0;
    int window_height = 0;

    EventQueue *event_queue = nullptr;

    ClipboardImage *clip = nullptr;

    bool mouse_captured = false;
    bool old_mouse_captured = false;
    
    SDL_Texture *last_texture = nullptr;
    SDL_FRect last_srcrect = { 0.0f, 0.0f, 0.0f, 0.0f };

    RGBA_t mono_color_table[DM_NUM_MONO_MODES] = {
        RGBA_t::make(0xFF, 0xFF, 0xFF), // white
        RGBA_t::make(0x00, 0xFF, 0x4A), // green (was 55) chosen from measuring @ 549nm
        RGBA_t::make(0xFF, 0xBF, 0x00)  // amber
    };

protected:
    void calculate_target_rect(int new_w, int new_h);
    // Recompute the target rect from the renderer's real pixel output size
    // (not window points), so the emulator image is sized for the high-DPI backbuffer.
    void update_target_from_output();
    // Create the CRT fragment shader and its GPU render state. No-op (returns
    // false) when the GPU renderer is not in use. Safe to call once at init.
    bool init_crt_shader();
    // Create/recreate the offscreen scene_target to match (w x h) pixels. No-op
    // when the CRT shader is unavailable. Called at init and on resize.
    void ensure_scene_target(int w, int h);

public:
    video_system_t(computer_t *computer);
    ~video_system_t();
    void set_window_title(const char *title);
    void window_resize(const SDL_Event &event);
    void toggle_fullscreen();
    void set_window_fullscreen(display_fullscreen_mode_t mode);
    display_fullscreen_mode_t get_window_fullscreen();
    void sync_window();
    void render_frame(SDL_Texture *texture, SDL_FRect *srcrect, SDL_FRect *dstadj, bool respect_mode = true );
    void clear();
    void present();
    bool display_capture_mouse(bool capture);
    bool display_capture_mouse_message(bool capture);
    bool is_mouse_captured();
    void raise();
    void raise(SDL_Window *window);
    void hide(SDL_Window *window);
    void show(SDL_Window *window);
    void send_engine_message();
    void toggle_display_engine();
    void set_display_engine(display_color_engine_t mode);
    void set_display_mono_color(display_mono_color_t mode);
    void copy_screen();
    // Render the current emulator frame and write it to a PNG file. Works in
    // both headed and headless modes: in headless mode `renderer` is a software
    // renderer backed by headless_surface, so the same frame_handlers draw path
    // and SDL_RenderReadPixels readback apply. Returns true on success.
    bool capture_png(const char *path);
    void flip_display_scale_mode();
    // True when the CRT post-process shader is available to be used.
    bool crt_shader_available() const { return crt_state != nullptr; }
    bool get_crt_shader_enabled() const { return crt_shader_enabled; }
    void toggle_crt_shader();
    void register_frame_processor(int weight, FrameHandler handler);
    void update_display(bool force_full_frame = false);
    // When the CRT shader is active, blit the offscreen scene_target onto the
    // swapchain (through the shader). No-op otherwise. Called once per frame
    // after update_display() and before the OSD is drawn.
    void present_scene();
    void push_mouse_capture(bool capture);
    void pop_mouse_capture();
    RGBA_t get_mono_color() { return mono_color_table[display_mono_color]; };
};
