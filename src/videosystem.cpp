//#include "gs2.hpp"
#include "computer.hpp"
#include "videosystem.hpp"
#include "display/display.hpp"
#include "ui/Clipboard.hpp"

video_system_t::video_system_t(computer_t *computer) {

    //SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    //SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
    }

    clip = new ClipboardImage();

    display_color_engine = DM_ENGINE_NTSC;
    display_mono_color = DM_MONO_GREEN;
    display_pixel_mode = DM_PIXEL_FUZZ;

    display_fullscreen_mode = DISPLAY_WINDOWED_MODE;
    event_queue = computer->event_queue;

    int window_width = (BASE_WIDTH + border_width*2) * SCALE_X;
    int window_height = (BASE_HEIGHT + border_height*2) * SCALE_Y;
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

    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        const char *name = SDL_GetRenderDriver(i);
        printf("Render driver %d: %s\n", i, name);
    }

    // Create renderer with nearest-neighbor scaling (sharp pixels)
    renderer = SDL_CreateRenderer(window, NULL );
    
    if (!renderer) {
        fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
    }

    const char *rname = SDL_GetRendererName(renderer);
    printf("Renderer: %s\n", rname);

    // Set scaling quality to nearest neighbor for sharp pixels
    /* SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); */
    SDL_SetRenderScale(renderer, SCALE_X, SCALE_Y);

    // Clear the texture to black
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    clear();
    present();
    force_full_frame_redraw = true; // first time through!

    SDL_RaiseWindow(window);

    computer->dispatch->registerHandler(SDL_EVENT_WINDOW_RESIZED, [this](const SDL_Event &event) {
        window_resize(event);
        return true;
    });
    computer->dispatch->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, [this](const SDL_Event &event) {
        if (display_capture_mouse(true)) {
            event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0, "Mouse Captured, release with F1"));
            //return true;
            return false;
        }
        return false;
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_DOWN, [this, computer](const SDL_Event &event) {
        int key = event.key.key;
        if (key == SDLK_F3) {
            toggle_fullscreen();
            return true;
        }
        if (key == SDLK_F1) {
            display_capture_mouse(false);
            return true;
        }
        if (key == SDLK_F5) {
            flip_display_scale_mode();
            return true;
        }
        if (key == SDLK_F2) {
            toggle_display_engine();
            set_full_frame_redraw();
            return true;
        }
        if (key == SDLK_PRINTSCREEN) {
            clip->Clip(computer);
            printf("click!\n");
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

void video_system_t::render_frame(SDL_Texture *texture, float offset) {
    float w,h;
    SDL_GetTextureSize(texture, &w, &h);
    float nw = w, nh = h;

    float xoffset = (offset/2.0f) * scale_x;
    if (nw == 640) { // TODO: kind of a cheesy hack to say "scale into the base display area"
        nw = BASE_WIDTH;
        nh = BASE_HEIGHT;
    }
    if (display_pixel_mode == DM_PIXEL_FUZZ) {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    } else {
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    }

    SDL_FRect dstrect = {
        (float)border_width + xoffset,
        (float)border_height,
       // (float)BASE_WIDTH, 
        //(float)BASE_HEIGHT
        nw, // should be minus 7 for Display, but no change for videx/GS.
        nh
    };
    SDL_FRect srcrect = {
        (float)0.0,
        (float)0.0,
        (float)w, 
        (float)h
    };
    SDL_RenderTexture(renderer, texture, &srcrect, &dstrect);
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

void video_system_t::window_resize(const SDL_Event &event) {
    // make sure this event is for the primary window.
    if (event.window.windowID != SDL_GetWindowID(window)) {
        return;
    }

    int new_w = event.window.data1;
    int new_h = event.window.data2;

    // Calculate new scale factors based on window size ratio
    float new_scale_x = (float)new_w / BASE_WIDTH;
    float new_scale_y = (float)new_h / (BASE_HEIGHT + border_height*2);
 
    // TODO: technically this works, but, we should adjust the border_width to center the image.
    // Means borders should be in variable in the display_state_t.
    if (new_scale_x > (new_scale_y / 2.0f)) {
        new_scale_x = new_scale_y / 2.0f;
    }

    border_width = ((new_w / new_scale_x)- BASE_WIDTH) / 2;

    printf("handle_window_resize: new_w: %d, new_h: %d new scale: %f, %f, border w: %d, h: %d\n", new_w, new_h, new_scale_x, new_scale_y, border_width, border_height);

    SDL_SetRenderScale(renderer, new_scale_x, new_scale_y);
    scale_x = new_scale_x;
    scale_y = new_scale_y;
}

display_fullscreen_mode_t video_system_t::get_window_fullscreen() {
    return display_fullscreen_mode;
}

void video_system_t::set_window_fullscreen(display_fullscreen_mode_t mode) {
    if (mode == DISPLAY_FULLSCREEN_MODE) {
        SDL_DisplayMode *selected_mode;

        SDL_DisplayID did = SDL_GetDisplayForWindow(window);
        int num_modes;
        SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(did, &num_modes);
        for (int i = 0; i < num_modes; i++) {
            printf("Mode %d: %dx%d\n", i, modes[i]->w, modes[i]->h);
        }
        selected_mode = modes[0];

        SDL_SetWindowAspectRatio(window, 0.0f, 0.0f);
        SDL_SetWindowFullscreenMode(window, selected_mode);
        SDL_SetWindowBordered(window, false);
        SDL_SetWindowFullscreen(window, display_fullscreen_mode);
        SDL_free(modes);
    } else {
        // Reapply window size and aspect ratio constraints in reverse order from above.
        SDL_SetWindowFullscreen(window, display_fullscreen_mode);
        SDL_SetWindowAspectRatio(window, aspect_ratio, aspect_ratio);
        SDL_SetWindowBordered(window, true);
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
    if (capture && SDL_GetWindowRelativeMouseMode(window)) { // TODO: this was keyboard grab, which is wrong.
        return false;
    }
    SDL_SetWindowKeyboardGrab(window, capture);
    SDL_SetWindowRelativeMouseMode(window, capture);
    return true;
}

void video_system_t::set_full_frame_redraw() {
    force_full_frame_redraw = true;
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
    set_full_frame_redraw();
    send_engine_message();
}

void video_system_t::set_display_engine(display_color_engine_t mode) {
    display_color_engine = mode;
    set_full_frame_redraw();
    send_engine_message();
}

void video_system_t::set_display_mono_color(display_mono_color_t mode) {
    display_mono_color = mode;
    set_full_frame_redraw();
}

void video_system_t::flip_display_scale_mode() {
    SDL_ScaleMode scale_mode;

    if (display_pixel_mode == DM_PIXEL_FUZZ) {
        display_pixel_mode = DM_PIXEL_SQUARE;
        scale_mode = SDL_SCALEMODE_NEAREST;
    } else {
        display_pixel_mode = DM_PIXEL_FUZZ;
        scale_mode = SDL_SCALEMODE_LINEAR;
    }
    set_full_frame_redraw();
}

void video_system_t::register_frame_processor(int weight, FrameHandler handler) {
    frame_handlers.insert({weight, handler});
}

void video_system_t::update_display() {
    clear(); // clear the backbuffer.

    for (const auto& pair : frame_handlers) {
        if (pair.second()) {
            break; // Stop processing if handler returns true
        }
    }
}
