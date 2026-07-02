#include "gs2.hpp"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_mouse.h"
#include "computer.hpp"
#include "videosystem.hpp"
#include "devices/adb/keygloo.hpp"
#include "display/display.hpp"
#include "ui/Clipboard.hpp"
#include <cmath>
#include "util/dialog.hpp"
#include "display/shaders/crt.frag.msl.h"

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
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
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

    // Optionally use the SDL GPU-backed renderer so we can attach custom
    // fragment shaders (CRT post-processing). On macOS this maps to Metal/MSL.
    // This is opt-in via the -g command-line flag because it changes high-DPI
    // backbuffer behavior; the default is the classic SDL 2D renderer.
    if (gs2_app_values.gpu_render) {
        renderer = SDL_CreateGPURenderer(window, SDL_GPU_SHADERFORMAT_MSL, &gpu_device);
        if (!renderer) {
            printf("GPU renderer unavailable (%s); falling back to classic renderer\n", SDL_GetError());
            gpu_device = nullptr;
        }
    }
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, NULL);
    }

    if (!renderer) {
        fprintf(stderr, "Error creating renderer: %s\n", SDL_GetError());
    }

    const char *rname = SDL_GetRendererName(renderer);
    printf("Renderer: %s (GPU device: %s)\n", rname, gpu_device ? "yes" : "no");

    init_crt_shader();

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

    {
        int point_w = 0, point_h = 0;
        int pixel_w = 0, pixel_h = 0;
        SDL_GetWindowSize(window, &point_w, &point_h);
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        float display_scale = SDL_GetWindowDisplayScale(window);
        float pixel_density = SDL_GetWindowPixelDensity(window);
        printf("Display: %dx%d points, %dx%d pixels, display_scale=%.3f, pixel_density=%.3f\n",
            point_w, point_h, pixel_w, pixel_h, display_scale, pixel_density);
    }

    update_target_from_output();

    computer->dispatch->registerHandler(SDL_EVENT_WINDOW_RESIZED, [this](const SDL_Event &event) {
        window_resize(event);
        return true;
    });
    computer->dispatch->registerHandler(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, [this](const SDL_Event &event) {
        window_resize(event);
        return true;
    });
    computer->sys_event->registerHandler(SDL_EVENT_KEY_UP, [this, computer](const SDL_Event &event) {
        int key = event.key.key;
        if (key == SDLK_F3) {
            toggle_fullscreen();
            return true;
        }
        if (key == SDLK_F1) {
            // F1 round-robins through MouseMode: FOLLOW_HOST -> CAPTURE
            // -> DISABLED -> FOLLOW_HOST. Replaces the older binary
            // capture toggle. The mode-set helper handles the SDL
            // relative-mode flip when entering or leaving CAPTURE and
            // shows an on-screen toast for the new mode.
            keygloo_cycle_mouse_mode(computer);
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
        if (key == SDLK_F7) {
            toggle_crt_shader();
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
            case SDLK_F6:
                return true; // eat the keydown
            case SDLK_PRINTSCREEN:
                copy_screen();
                return true;
            default:
                return false;
        }
    });
    computer->sys_event->registerHandler(SDL_EVENT_MOUSE_BUTTON_DOWN, [this, computer](const SDL_Event &event) {
        if (event.button.button == SDL_BUTTON_MIDDLE) {
            keygloo_cycle_mouse_mode(computer);
        }
        return false;
    });
}

video_system_t::~video_system_t() {
    if (scene_target) SDL_DestroyTexture(scene_target);
    if (crt_state) SDL_DestroyGPURenderState(crt_state);
    if (crt_shader && gpu_device) SDL_ReleaseGPUShader(gpu_device, crt_shader);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (clip) delete clip;
    SDL_Quit();
}

// Fragment-shader uniform block for the CRT effect (matches the shader's
// cbuffer: float2 resolution).
struct crt_uniforms_t {
    float texture_width;
    float texture_height;
};

bool video_system_t::init_crt_shader() {
    if (!gpu_device) {
        return false; // classic renderer: no shader support.
    }

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(gpu_device);
    if (!(formats & SDL_GPU_SHADERFORMAT_MSL)) {
        printf("CRT shader: MSL format not supported by GPU device; shader disabled\n");
        return false;
    }

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.format = SDL_GPU_SHADERFORMAT_MSL;
    info.code = testgpu_effects_CRT_frag_msl;
    info.code_size = testgpu_effects_CRT_frag_msl_len;
    info.num_samplers = 1;
    info.num_uniform_buffers = 1;
    info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;

    crt_shader = SDL_CreateGPUShader(gpu_device, &info);
    if (!crt_shader) {
        printf("CRT shader: SDL_CreateGPUShader failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPURenderStateDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.fragment_shader = crt_shader;
    crt_state = SDL_CreateGPURenderState(renderer, &desc);
    if (!crt_state) {
        printf("CRT shader: SDL_CreateGPURenderState failed: %s\n", SDL_GetError());
        SDL_ReleaseGPUShader(gpu_device, crt_shader);
        crt_shader = nullptr;
        return false;
    }

    printf("CRT shader: initialized\n");
    return true;
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

void video_system_t::update_target_from_output() {
    int pixel_w = 0, pixel_h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &pixel_w, &pixel_h);
    calculate_target_rect(pixel_w, pixel_h);
    ensure_scene_target(pixel_w, pixel_h);
}

void video_system_t::ensure_scene_target(int w, int h) {
    if (!crt_state) {
        return; // shader unavailable: scene_target is never used.
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    if (scene_target && scene_target_w == w && scene_target_h == h) {
        return; // already the right size.
    }
    if (scene_target) {
        SDL_DestroyTexture(scene_target);
        scene_target = nullptr;
    }
    scene_target = SDL_CreateTexture(renderer, PIXEL_FORMAT, SDL_TEXTUREACCESS_TARGET, w, h);
    if (!scene_target) {
        printf("CRT shader: failed to create scene_target %dx%d: %s\n", w, h, SDL_GetError());
        scene_target_w = scene_target_h = 0;
        return;
    }
    SDL_SetTextureBlendMode(scene_target, SDL_BLENDMODE_NONE);
    scene_target_w = w;
    scene_target_h = h;
}

void video_system_t::window_resize(const SDL_Event &event) {
    if (event.window.windowID != SDL_GetWindowID(window)) {
        return;
    }
    // The emulator renders in real output pixels, so size the target from the
    // renderer's pixel output rather than the event's point dimensions.
    update_target_from_output();
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

void video_system_t::toggle_crt_shader() {
    if (!crt_shader_available()) {
        event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0,
            "CRT Shader unavailable (start with -g for GPU rendering)"));
        return;
    }
    crt_shader_enabled = !crt_shader_enabled;
    event_queue->addEvent(new Event(EVENT_SHOW_MESSAGE, 0,
        crt_shader_enabled ? "CRT Shader On" : "CRT Shader Off"));
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
    // When the CRT shader is active, draw the emulator frame into the offscreen
    // scene_target so it can be post-processed during present_scene(). Otherwise
    // draw straight to the swapchain exactly as before.
    bool use_scene = crt_shader_enabled && crt_state && scene_target;
    if (use_scene) {
        SDL_SetRenderTarget(renderer, scene_target);
    }

    clear(); // clear the current render target (scene_target or swapchain).

    for (const auto& pair : frame_handlers) {
        if (pair.second(force_full_frame)) {
            break; // Stop processing if handler returns true
        }
    }

    if (use_scene) {
        SDL_SetRenderTarget(renderer, nullptr);
    }
}

void video_system_t::present_scene() {
    if (!(crt_shader_enabled && crt_state && scene_target)) {
        return; // shader disabled/unavailable: update_display drew to the swapchain.
    }
    // Feed the CRT shader the resolution it samples against.
    //   x  -> phosphor mask frequency: keep at output pixels so the grille
    //         stays a crisp few-pixel cell.
    //   y  -> scanline frequency: the shader produces resolution.y/2 bands, so
    //         drive it off the emulated visible line count (not the pixel
    //         height) to get ~one scanline per source line. Apple II shows ~192
    //         visible lines; *2 gives one bright band per line. Bump/lower this
    //         constant to taste (smaller = fatter, more visible scanlines).
    const float source_lines = 192.0f;
    crt_uniforms_t uniforms;
    uniforms.texture_width = (float)scene_target_w;
    uniforms.texture_height = source_lines * 2.0f;
    SDL_SetGPURenderStateFragmentUniforms(crt_state, 0, &uniforms, sizeof(uniforms));

    // Blit the offscreen scene onto the swapchain 1:1 through the CRT shader.
    SDL_SetRenderGPUState(renderer, crt_state);
    SDL_RenderTexture(renderer, scene_target, nullptr, nullptr);
    SDL_SetRenderGPUState(renderer, nullptr);
}
