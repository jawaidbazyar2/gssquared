//#include "gs2.hpp"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_mouse.h"
#include "computer.hpp"
#include "videosystem.hpp"
#include "display/display.hpp"
#include "ui/Clipboard.hpp"
#include <cmath>
#include "util/dialog.hpp"

video_system_t::video_system_t(computer_t *computer) {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
    }

    clip = new ClipboardImage();

    display_color_engine = DM_ENGINE_NTSC;
    display_mono_color = DM_MONO_GREEN;
    display_pixel_mode = DM_PIXEL_FUZZ;

    display_fullscreen_mode = DISPLAY_WINDOWED_MODE;
    event_queue = computer->event_queue;

    // TODO: calculate an initial window size that will get us an integral scale starting out.
    window_width = (BASE_WIDTH + border_width*2) * SCALE_X;
    window_height = (BASE_HEIGHT + border_height*2) * SCALE_Y;
    aspect_ratio = (float)window_width / (float)window_height;

    window = SDL_CreateWindow(
        "GSSquared - Apple ][ Emulator", 
        (BASE_WIDTH + border_width*2) * SCALE_X, 
        (BASE_HEIGHT + border_height*2) * SCALE_Y, 
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        fprintf(stderr, "Window could not be created! SDL_Error: %s\n", SDL_GetError());
    }

    // Set minimum and maximum window sizes to maintain reasonable dimensions
    SDL_SetWindowMinimumSize(window, window_width / 2, window_height / 2);  // Half size
    //SDL_SetWindowMaximumSize(window, window_width * 2, window_height * 2);  // 4x size
    
    // Set the window's aspect ratio to match the Apple II display (560:384)
    SDL_SetWindowAspectRatio(window, aspect_ratio, aspect_ratio);

    /* for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        const char *name = SDL_GetRenderDriver(i);
        printf("Render driver %d: %s\n", i, name);
    } */

    // Create renderer with nearest-neighbor scaling (sharp pixels)
    renderer = SDL_CreateRenderer(window, NULL );
    
    if (!renderer) {
        fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
    }

    const char *rname = SDL_GetRendererName(renderer);
    printf("Renderer: %s\n", rname);

    screencap_texture = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_TARGET, 910, 263);
    if (!screencap_texture) {
        printf("Failed to create txt_shr\n");
        printf("SDL Error: %s\n", SDL_GetError());
        system_failure("Failed to create screencap texture");
    }

    // Set scaling quality to nearest neighbor for sharp pixels
    //SDL_SetRenderScale(renderer, SCALE_X, SCALE_Y);

    // Clear the texture to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    clear();
    present();

    SDL_RaiseWindow(window);

    calculate_target_rect(window_width, window_height);

    computer->dispatch->registerHandler(SDL_EVENT_WINDOW_RESIZED, [this](const SDL_Event &event) {
        window_resize(event);
        return true;
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_UP, [this, computer](const SDL_Event &event) {
        int key = event.key.key;
        if (key == SDLK_F3) {
            toggle_fullscreen();
            return true;
        }
        if (key == SDLK_F1) { // release or capture mouse
            display_capture_mouse_message(!mouse_captured);
            return true;
        }
        if (key == SDLK_F5) {
            flip_display_scale_mode();
            return true;
        }
        if (key == SDLK_F2) {
            toggle_display_engine();
            return true;
        }
        if (key == SDLK_PRINTSCREEN) {
            copy_screen();
        }
        return false;
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [this, computer](const SDL_Event &event) {
        int key = event.key.key;
        switch (key) {
            case SDLK_F3:
            case SDLK_F1:
            case SDLK_F5:
            case SDLK_F2:
                return true; // eat the keydown
            case SDLK_PRINTSCREEN:
                copy_screen();
                return true;
            default:
                return false;
        }
    });
    computer->sys_event->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, [this](const SDL_Event &event) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            display_capture_mouse_message(!mouse_captured);
        }
        return false;
    });
}

video_system_t::~video_system_t() {
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (clip) delete clip;
    SDL_Quit();
}

void video_system_t::present() {
    SDL_RenderPresent(renderer);
}

void video_system_t::set_window_title(const char *title) {
    SDL_SetWindowTitle(window, title);
}

void video_system_t::render_frame(SDL_Texture *texture, SDL_FRect *srcrect, SDL_FRect *dstadj, bool respect_mode) {

    SDL_FRect adj_target;
    if (dstadj) {
        float scale_x = target.w / srcrect->w; // recalc the scale
        float scale_y = target.h / srcrect->h; // recalc the scale
        
        float xadj = dstadj->w * scale_x;
        float yadj = dstadj->h * scale_y;
        adj_target.x = target.x + xadj;
        adj_target.y = target.y + yadj;
        adj_target.w = target.w - xadj*2;
        adj_target.h = target.h - yadj*2;
    } else {
        adj_target = target;
    }

    if (respect_mode) {
        if (display_pixel_mode == DM_PIXEL_FUZZ) {
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
        } else {
            SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART); // SDL_SCALEMODE_NEAREST
        }
    }

    SDL_RenderTexture(renderer, texture, srcrect, &adj_target);
    last_texture = texture;
    last_srcrect = *srcrect;
}

void video_system_t::clear() {
    SDL_RenderClear(renderer);
}

void video_system_t::raise() {
    SDL_RaiseWindow(window);
}
void video_system_t::raise(SDL_Window *windowp) {
    SDL_RaiseWindow(windowp);
}

void video_system_t::hide(SDL_Window *window) {
    SDL_HideWindow(window);
}

void video_system_t::show(SDL_Window *window) {
    SDL_ShowWindow(window);
}

/* Given new window width and height, calculate the target rectangle for the display. */
void video_system_t::calculate_target_rect(int new_w, int new_h) {
    float new_aspect = (float)new_w / new_h;
    constexpr float aspect_epsilon = 0.001f;
    if (std::fabs(new_aspect - aspect_ratio) <= aspect_epsilon) {
        printf("aspect match ");   
        // how accurate will a float be here?
        target.w = new_w;
        target.h = new_h;
        target.x = 0.0f;
        target.y = 0.0f;
    } else if (new_aspect > aspect_ratio) { 
        printf("aspect wide ");
        // wider than our aspect ratio.
        target.h = new_h;
        target.w = (float) new_h * aspect_ratio;        
        target.y = 0.0f;
        target.x = ((float)new_w - target.w) / 2.0f;
    } else {
        // narrower than our aspect ratio.
        printf("aspect tall ");
        target.w = new_w;
        target.h = (float) new_w / aspect_ratio;
        target.x = 0.0f;
        target.y = ((float)new_h - target.h) / 2.0f;
    }
    printf("calculate_target_rect: (%f, %f) [%f x %f] @ %f\n", target.x, target.y, target.w, target.h, (float)target.w/target.h);
}

void video_system_t::window_resize(const SDL_Event &event) {
    if (event.window.windowID != SDL_GetWindowID(window)) {
        return;
    }
    calculate_target_rect(event.window.data1, event.window.data2);
}

display_fullscreen_mode_t video_system_t::get_window_fullscreen() {
    return display_fullscreen_mode;
}

void video_system_t::set_window_fullscreen(display_fullscreen_mode_t mode) {
    if (mode == DISPLAY_FULLSCREEN_MODE) {
        // Borderless "fullscreen desktop": a NULL fullscreen mode tells SDL not to
        // change the display's video mode, so the window simply covers the desktop
        // at its current resolution. This avoids the slow monitor re-sync / macOS
        // Space transition that an exclusive mode switch incurs.
        SDL_SetWindowAspectRatio(window, 0.0f, 0.0f);
        SDL_SetWindowFullscreenMode(window, NULL);
        SDL_SetWindowBordered(window, false);
        SDL_SetWindowFullscreen(window, true);
    } else {
        // Reapply window size and aspect ratio constraints in reverse order from above.
        SDL_SetWindowFullscreen(window, false);
        SDL_SetWindowBordered(window, true);
        sync_window();
        SDL_SetWindowAspectRatio(window, aspect_ratio, aspect_ratio);
    } 
}

void video_system_t::sync_window() {
    SDL_SyncWindow(window);
}

void video_system_t::toggle_fullscreen() {
    display_fullscreen_mode = (display_fullscreen_mode_t)((display_fullscreen_mode + 1) % NUM_FULLSCREEN_MODES);
    set_window_fullscreen(display_fullscreen_mode);
}

bool video_system_t::display_capture_mouse(bool capture) {
    printf("display_capture_mouse: %d\n", capture);
    mouse_captured = capture;
    if (!SDL_SetWindowRelativeMouseMode(window, capture)) {
        printf("SDL_SetWindowRelativeMouseMode failed: %s\n", SDL_GetError());
    }
    if (!SDL_SetWindowMouseGrab(window, capture)) {
        printf("SDL_SetWindowMouseGrab failed: %s\n", SDL_GetError());
    }
    if (!SDL_SetWindowKeyboardGrab(window, capture)) {
        printf("SDL_SetWindowKeyboardGrab failed: %s\n", SDL_GetError());
    }
    return capture;
}

bool video_system_t::display_capture_mouse_message(bool capture) {
    bool oldstate = mouse_captured;
    bool result = display_capture_mouse(capture);
    if (!oldstate) {
        event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Mouse Captured, release with F1"));
    }
    return true;
}

void video_system_t::push_mouse_capture(bool capture) {
    old_mouse_captured = mouse_captured;
    display_capture_mouse(capture);
}

void video_system_t::pop_mouse_capture() {
    display_capture_mouse(old_mouse_captured);
}

void video_system_t::send_engine_message() {
    static char buffer[256];
    const char *display_color_engine_names[] = {
        "NTSC",
        "RGB",
        "Monochrome"
    };

    snprintf(buffer, sizeof(buffer), "Display Engine Set to %s", display_color_engine_names[display_color_engine]);
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, buffer));
}

void video_system_t::toggle_display_engine() {
    display_color_engine = (display_color_engine_t)((display_color_engine + 1) % DM_NUM_COLOR_ENGINES);
    send_engine_message();
}

void video_system_t::set_display_engine(display_color_engine_t mode) {
    display_color_engine = mode;
    send_engine_message();
}

void video_system_t::set_display_mono_color(display_mono_color_t mode) {
    display_mono_color = mode;
}

void video_system_t::flip_display_scale_mode() {
    SDL_ScaleMode scale_mode;

    if (display_pixel_mode == DM_PIXEL_FUZZ) {
        display_pixel_mode = DM_PIXEL_SQUARE;
        scale_mode = SDL_SCALEMODE_PIXELART;
    } else {
        display_pixel_mode = DM_PIXEL_FUZZ;
        scale_mode = SDL_SCALEMODE_LINEAR;
    }
}

void video_system_t::copy_screen() {
    SDL_Rect srect = { (int)last_srcrect.x, (int)last_srcrect.y, (int)last_srcrect.w, (int)last_srcrect.h };
    SDL_FRect trect = { last_srcrect.x, last_srcrect.y, last_srcrect.w, last_srcrect.h };
    //SDL_SetTextureBlendMode(screencap_texture, SDL_BLENDMODE_NONE);
    SDL_SetRenderTarget(renderer, screencap_texture);
    SDL_SetTextureBlendMode(last_texture, SDL_BLENDMODE_NONE);
    // oops, this is scaling the image.
    SDL_RenderTexture(renderer, last_texture, &trect, &trect); // ensure no scaling.
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, &srect);
    SDL_SetRenderTarget(renderer, nullptr);
    clip->Clip(surface);
    SDL_DestroySurface(surface);
}

void video_system_t::register_frame_processor(int weight, FrameHandler handler) {
    frame_handlers.insert({weight, handler});
}

void video_system_t::update_display(bool force_full_frame) {
    clear(); // clear the backbuffer.

    for (const auto& pair : frame_handlers) {
        if (pair.second(force_full_frame)) {
            break; // Stop processing if handler returns true
        }
    }
}
