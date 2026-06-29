// imgui_impl_sdl3.cpp – minimal SDL3 platform backend for Dear ImGui 1.90.1
// Written against SDL3 3.3.0 API. Implements the same public interface as the
// upstream imgui_impl_sdl3 so that imgui_impl_sdl3.h (system header) is satisfied.

#include "imgui.h"
#ifndef IMGUI_DISABLE
#include "backends/imgui_impl_sdl3.h"

#include <SDL3/SDL.h>
#include <cstring>

// ── Backend data ──────────────────────────────────────────────────────────────

struct ImGui_ImplSDL3_Data {
    SDL_Window*  Window;
    SDL_Renderer* Renderer;
    Uint64        Time;
    SDL_Cursor*   MouseCursors[ImGuiMouseCursor_COUNT];
    SDL_Cursor*   LastMouseCursor;
    char*         ClipboardTextData;

    ImGui_ImplSDL3_Data() { memset((void*)this, 0, sizeof(*this)); }
};

static ImGui_ImplSDL3_Data* ImGui_ImplSDL3_GetBackendData()
{
    return ImGui::GetCurrentContext()
        ? (ImGui_ImplSDL3_Data*)ImGui::GetIO().BackendPlatformUserData
        : nullptr;
}

// ── Clipboard ─────────────────────────────────────────────────────────────────

static const char* ImGui_ImplSDL3_GetClipboardText(void*)
{
    ImGui_ImplSDL3_Data* bd = ImGui_ImplSDL3_GetBackendData();
    if (bd->ClipboardTextData) SDL_free(bd->ClipboardTextData);
    bd->ClipboardTextData = SDL_GetClipboardText();
    return bd->ClipboardTextData;
}

static void ImGui_ImplSDL3_SetClipboardText(void*, const char* text)
{
    SDL_SetClipboardText(text);
}

// ── Key mapping ───────────────────────────────────────────────────────────────

static ImGuiKey ImGui_ImplSDL3_KeycodeToImGuiKey(SDL_Keycode keycode)
{
    switch (keycode) {
        case SDLK_TAB:          return ImGuiKey_Tab;
        case SDLK_LEFT:         return ImGuiKey_LeftArrow;
        case SDLK_RIGHT:        return ImGuiKey_RightArrow;
        case SDLK_UP:           return ImGuiKey_UpArrow;
        case SDLK_DOWN:         return ImGuiKey_DownArrow;
        case SDLK_PAGEUP:       return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN:     return ImGuiKey_PageDown;
        case SDLK_HOME:         return ImGuiKey_Home;
        case SDLK_END:          return ImGuiKey_End;
        case SDLK_INSERT:       return ImGuiKey_Insert;
        case SDLK_DELETE:       return ImGuiKey_Delete;
        case SDLK_BACKSPACE:    return ImGuiKey_Backspace;
        case SDLK_SPACE:        return ImGuiKey_Space;
        case SDLK_RETURN:       return ImGuiKey_Enter;
        case SDLK_ESCAPE:       return ImGuiKey_Escape;
        case SDLK_APOSTROPHE:   return ImGuiKey_Apostrophe;
        case SDLK_COMMA:        return ImGuiKey_Comma;
        case SDLK_MINUS:        return ImGuiKey_Minus;
        case SDLK_PERIOD:       return ImGuiKey_Period;
        case SDLK_SLASH:        return ImGuiKey_Slash;
        case SDLK_SEMICOLON:    return ImGuiKey_Semicolon;
        case SDLK_EQUALS:       return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET:  return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH:    return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_GRAVE:        return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK:     return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK:   return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN:  return ImGuiKey_PrintScreen;
        case SDLK_PAUSE:        return ImGuiKey_Pause;
        case SDLK_KP_0:         return ImGuiKey_Keypad0;
        case SDLK_KP_1:         return ImGuiKey_Keypad1;
        case SDLK_KP_2:         return ImGuiKey_Keypad2;
        case SDLK_KP_3:         return ImGuiKey_Keypad3;
        case SDLK_KP_4:         return ImGuiKey_Keypad4;
        case SDLK_KP_5:         return ImGuiKey_Keypad5;
        case SDLK_KP_6:         return ImGuiKey_Keypad6;
        case SDLK_KP_7:         return ImGuiKey_Keypad7;
        case SDLK_KP_8:         return ImGuiKey_Keypad8;
        case SDLK_KP_9:         return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD:    return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE:    return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY:  return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS:     return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS:      return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER:     return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS:    return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL:        return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT:       return ImGuiKey_LeftShift;
        case SDLK_LALT:         return ImGuiKey_LeftAlt;
        case SDLK_LGUI:         return ImGuiKey_LeftSuper;
        case SDLK_RCTRL:        return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT:       return ImGuiKey_RightShift;
        case SDLK_RALT:         return ImGuiKey_RightAlt;
        case SDLK_RGUI:         return ImGuiKey_RightSuper;
        case SDLK_APPLICATION:  return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0; case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2; case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4; case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6; case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8; case SDLK_9: return ImGuiKey_9;
        case SDLK_A: return ImGuiKey_A; case SDLK_B: return ImGuiKey_B;
        case SDLK_C: return ImGuiKey_C; case SDLK_D: return ImGuiKey_D;
        case SDLK_E: return ImGuiKey_E; case SDLK_F: return ImGuiKey_F;
        case SDLK_G: return ImGuiKey_G; case SDLK_H: return ImGuiKey_H;
        case SDLK_I: return ImGuiKey_I; case SDLK_J: return ImGuiKey_J;
        case SDLK_K: return ImGuiKey_K; case SDLK_L: return ImGuiKey_L;
        case SDLK_M: return ImGuiKey_M; case SDLK_N: return ImGuiKey_N;
        case SDLK_O: return ImGuiKey_O; case SDLK_P: return ImGuiKey_P;
        case SDLK_Q: return ImGuiKey_Q; case SDLK_R: return ImGuiKey_R;
        case SDLK_S: return ImGuiKey_S; case SDLK_T: return ImGuiKey_T;
        case SDLK_U: return ImGuiKey_U; case SDLK_V: return ImGuiKey_V;
        case SDLK_W: return ImGuiKey_W; case SDLK_X: return ImGuiKey_X;
        case SDLK_Y: return ImGuiKey_Y; case SDLK_Z: return ImGuiKey_Z;
        case SDLK_F1:  return ImGuiKey_F1;  case SDLK_F2:  return ImGuiKey_F2;
        case SDLK_F3:  return ImGuiKey_F3;  case SDLK_F4:  return ImGuiKey_F4;
        case SDLK_F5:  return ImGuiKey_F5;  case SDLK_F6:  return ImGuiKey_F6;
        case SDLK_F7:  return ImGuiKey_F7;  case SDLK_F8:  return ImGuiKey_F8;
        case SDLK_F9:  return ImGuiKey_F9;  case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11; case SDLK_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

static void ImGui_ImplSDL3_UpdateKeyModifiers(SDL_Keymod sdl_key_mods)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl,  (sdl_key_mods & SDL_KMOD_CTRL)  != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (sdl_key_mods & SDL_KMOD_SHIFT) != 0);
    io.AddKeyEvent(ImGuiMod_Alt,   (sdl_key_mods & SDL_KMOD_ALT)   != 0);
    io.AddKeyEvent(ImGuiMod_Super, (sdl_key_mods & SDL_KMOD_GUI)   != 0);
}

// ── Public API ────────────────────────────────────────────────────────────────

bool ImGui_ImplSDL3_InitForSDLRenderer(SDL_Window* window, SDL_Renderer* renderer)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized!");

    ImGui_ImplSDL3_Data* bd = IM_NEW(ImGui_ImplSDL3_Data)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName     = "imgui_impl_sdl3";

    bd->Window   = window;
    bd->Renderer = renderer;
    bd->Time     = 0;

    io.SetClipboardTextFn = ImGui_ImplSDL3_SetClipboardText;
    io.GetClipboardTextFn = ImGui_ImplSDL3_GetClipboardText;
    io.ClipboardUserData  = nullptr;

    // Create mouse cursors
    bd->MouseCursors[ImGuiMouseCursor_Arrow]      = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
    bd->MouseCursors[ImGuiMouseCursor_TextInput]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
    bd->MouseCursors[ImGuiMouseCursor_ResizeAll]  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNS]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
    bd->MouseCursors[ImGuiMouseCursor_ResizeEW]   = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
    bd->MouseCursors[ImGuiMouseCursor_ResizeNWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
    bd->MouseCursors[ImGuiMouseCursor_Hand]       = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
    bd->MouseCursors[ImGuiMouseCursor_NotAllowed] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);

    return true;
}

void ImGui_ImplSDL3_Shutdown()
{
    ImGui_ImplSDL3_Data* bd = ImGui_ImplSDL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "No platform backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    if (bd->ClipboardTextData) SDL_free(bd->ClipboardTextData);
    for (ImGuiMouseCursor cursor_n = 0; cursor_n < ImGuiMouseCursor_COUNT; cursor_n++)
        SDL_DestroyCursor(bd->MouseCursors[cursor_n]);

    io.BackendPlatformName     = nullptr;
    io.BackendPlatformUserData = nullptr;
    IM_DELETE(bd);
}

static void ImGui_ImplSDL3_UpdateMouseCursor()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) return;

    ImGui_ImplSDL3_Data* bd = ImGui_ImplSDL3_GetBackendData();
    ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor || imgui_cursor == ImGuiMouseCursor_None) {
        SDL_HideCursor();
    } else {
        SDL_Cursor* expected = bd->MouseCursors[imgui_cursor]
            ? bd->MouseCursors[imgui_cursor]
            : bd->MouseCursors[ImGuiMouseCursor_Arrow];
        if (bd->LastMouseCursor != expected) {
            SDL_SetCursor(expected);
            bd->LastMouseCursor = expected;
        }
        SDL_ShowCursor();
    }
}

void ImGui_ImplSDL3_NewFrame()
{
    ImGui_ImplSDL3_Data* bd = ImGui_ImplSDL3_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplSDL3_Init*?");
    ImGuiIO& io = ImGui::GetIO();

    // Window size (in points) and the backbuffer's pixel density. On a high-DPI
    // window the pixel size is larger than the point size, so derive the
    // framebuffer scale from their ratio instead of assuming 1.0.
    int w, h;
    SDL_GetWindowSize(bd->Window, &w, &h);
    int pixel_w = w, pixel_h = h;
    SDL_GetWindowSizeInPixels(bd->Window, &pixel_w, &pixel_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(
        w > 0 ? (float)pixel_w / (float)w : 1.0f,
        h > 0 ? (float)pixel_h / (float)h : 1.0f);

    // Delta time
    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 current   = SDL_GetPerformanceCounter();
    if (bd->Time > 0)
        io.DeltaTime = (float)((double)(current - bd->Time) / (double)frequency);
    else
        io.DeltaTime = 1.0f / 60.0f;
    bd->Time = current;

    // Mouse position — use global mouse state mapped to window coordinates
    {
        float mx, my;
        SDL_GetGlobalMouseState(&mx, &my);
        int wx, wy;
        SDL_GetWindowPosition(bd->Window, &wx, &wy);
        io.AddMousePosEvent(mx - (float)wx, my - (float)wy);
    }

    ImGui_ImplSDL3_UpdateMouseCursor();
}

bool ImGui_ImplSDL3_ProcessEvent(const SDL_Event* event)
{
    ImGuiIO& io = ImGui::GetIO();

    switch (event->type) {
        case SDL_EVENT_MOUSE_MOTION: {
            io.AddMousePosEvent(event->motion.x, event->motion.y);
            return false;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int button = -1;
            if (event->button.button == SDL_BUTTON_LEFT)   button = 0;
            if (event->button.button == SDL_BUTTON_RIGHT)  button = 1;
            if (event->button.button == SDL_BUTTON_MIDDLE) button = 2;
            if (button != -1) {
                io.AddMouseButtonEvent(button, event->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                return io.WantCaptureMouse;
            }
            return false;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            io.AddMouseWheelEvent(event->wheel.x, event->wheel.y);
            return io.WantCaptureMouse;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            ImGui_ImplSDL3_UpdateKeyModifiers(event->key.mod);
            ImGuiKey key = ImGui_ImplSDL3_KeycodeToImGuiKey(event->key.key);
            io.AddKeyEvent(key, event->type == SDL_EVENT_KEY_DOWN);
            return io.WantCaptureKeyboard;
        }
        case SDL_EVENT_TEXT_INPUT: {
            io.AddInputCharactersUTF8(event->text.text);
            return io.WantCaptureKeyboard;
        }
        default:
            return false;
    }
}

#endif // #ifndef IMGUI_DISABLE
