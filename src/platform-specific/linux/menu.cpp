// Portable Dear ImGui menu shared by the Linux and Emscripten (web) builds.
// It only uses ImGui + the SDL3 renderer backend; nothing GTK/X11-specific.
#if defined(__linux__) || defined(__EMSCRIPTEN__)

#include <SDL3/SDL.h>

#include <string>

#include <imgui.h>
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlrenderer3.h"

#include "platform-specific/menu.h"
#include "util/MenuInterface.h"
#include "gs2.hpp"

static constexpr float MENU_FONT_SIZE = 20.0f;

// ── Module state ──────────────────────────────────────────────────────────────

static SDL_Window   *g_window        = nullptr;
static bool          g_imgui_inited  = false;

// Mouse grab state saved before we release for menu interaction
static bool          g_was_grab      = false;
static bool          g_was_relative  = false;
static bool          g_grab_released = false; // true while we hold the grab released for ImGui

// ── Helpers ───────────────────────────────────────────────────────────────────

static void release_grab()
{
    if (!g_grab_released) {
        g_was_grab     = SDL_GetWindowMouseGrab(g_window);
        g_was_relative = SDL_GetWindowRelativeMouseMode(g_window);
        if (g_was_grab) {
            SDL_SetWindowMouseGrab(g_window, false);
            SDL_CaptureMouse(false);
        }
        if (g_was_relative)
            SDL_SetWindowRelativeMouseMode(g_window, false);
        g_grab_released = true;
    }
}

static void restore_grab()
{
    if (g_grab_released) {
        if (g_was_grab) {
            SDL_SetWindowMouseGrab(g_window, true);
            SDL_CaptureMouse(true);
        }
        if (g_was_relative)
            SDL_SetWindowRelativeMouseMode(g_window, true);
        g_grab_released = false;
    }
}

/** True while the emulator holds relative mode / grab (Machine → Capture Mouse). */
static bool emulated_mouse_captured()
{
    return SDL_GetWindowMouseGrab(g_window) || SDL_GetWindowRelativeMouseMode(g_window);
}

// ── Menu rendering ────────────────────────────────────────────────────────────

static void render_drives_menu()
{
    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();
    auto drives  = mi->getDriveList();

    if (drives.empty()) {
        ImGui::BeginDisabled();
        ImGui::MenuItem("(no drives)");
        ImGui::EndDisabled();
    } else {
        for (size_t i = 0; i < drives.size(); ++i) {
            const MenuDriveInfo &info = drives[i];

            std::string label = "S" + std::to_string(info.key.slot)
                              + "D" + std::to_string(info.key.drive + 1);
            if (info.is_mounted && !info.filename.empty()) {
                std::string fname = info.filename;
                size_t pos = fname.find_last_of("/\\");
                if (pos != std::string::npos)
                    fname = fname.substr(pos + 1);
                label += ": " + fname;
                if (info.is_modified)        label += " *";
                if (info.is_write_protected) label += " [WP]";
            } else {
                label += ": (empty)";
            }

            // Unique id per item to avoid ImGui id collisions
            label += "##drive" + std::to_string(i);

            if (!running) ImGui::BeginDisabled();
            if (ImGui::MenuItem(label.c_str()))
                mi->diskToggle(drives[i].key);
            if (!running) ImGui::EndDisabled();
        }
    }
}

static void build_menu_bar()
{
    MenuInterface *mi      = getMenuInterface();
    bool           running = mi->isEmulationRunning();

    // ── File ─────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("File")) {
        if (ImGui::BeginMenu("Drives")) {
            render_drives_menu();
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Close Emulation")) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
        }
        ImGui::EndMenu();
    }

    // ── Edit ─────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Copy Screen"))  mi->editCopyScreen();
        if (ImGui::MenuItem("Paste Text"))   mi->editPasteText();
        ImGui::EndMenu();
    }

    // ── Machine ───────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Machine")) {
        if (!running) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Reset"))         mi->machineReset();
        if (ImGui::MenuItem("Restart"))       mi->machineRestart();
        if (ImGui::MenuItem("Pause / Resume")) mi->machinePauseResume();
        ImGui::Separator();
        if (ImGui::MenuItem("Capture Mouse")) mi->machineCaptureMouse();
        // Cycle through mouse-input modes. Build label dynamically so
        // it shows the current mode; clicking advances to the next.
        {
            std::string label = std::string("Mouse Mode: ") +
                                mi->getCurrentMouseModeLabel() + " (F1)";
            if (ImGui::MenuItem(label.c_str())) mi->machineCycleMouseMode();
        }
        if (!running) ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    // ── Settings ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Settings")) {
        if (!running) ImGui::BeginDisabled();

        // Speed submenu
        int cur_speed = running ? mi->getCurrentSpeed() : 0;
        if (ImGui::BeginMenu("Speed")) {
            struct { const char *label; int id; } speeds[] = {
                { "1.0 MHz",  SPEED_1_0  },
                { "2.8 MHz",  SPEED_2_8  },
                { "7.1 MHz",  SPEED_7_1  },
                { "14.3 MHz", SPEED_14_3 },
            };
            for (auto &s : speeds) {
                bool checked = (cur_speed == s.id);
                if (ImGui::MenuItem(s.label, nullptr, checked))
                    mi->setSpeed(s.id);
            }
            ImGui::EndMenu();
        }

        // Game Controller submenu
        int cur_ctrl = running ? mi->getCurrentControllerMode() : -1;
        if (ImGui::BeginMenu("Game Controller")) {
            struct { const char *label; int mode; } controllers[] = {
                { "Joystick - Gamepad",     0 },
                { "Joystick - Mouse",       1 },
                { "Sirius / Atari Joyport", 2 },
            };
            for (auto &c : controllers) {
                bool checked = (cur_ctrl == c.mode);
                if (ImGui::MenuItem(c.label, nullptr, checked))
                    mi->setControllerMode(c.mode);
            }
            ImGui::EndMenu();
        }

        if (!running) ImGui::EndDisabled();

        ImGui::Separator();

        // Sleep / Busy Wait — available regardless of running state
        bool sleep_on = mi->getSleepMode();
        if (ImGui::MenuItem("Sleep / Busy Wait", nullptr, sleep_on))
            mi->toggleSleepMode();

        // R-Channel Decorrelation — available regardless of running state
        bool ad_on = mi->getAudioDecorrelation();
        if (ImGui::MenuItem("Mono Helper", nullptr, ad_on))
            mi->toggleAudioDecorrelation();

        bool rmb_accel = mi->getRightMouseAccel();
        if (ImGui::MenuItem("Right Mouse Button Accelerate", nullptr, rmb_accel))
            mi->toggleRightMouseAccel();

        ImGui::EndMenu();
    }

    // ── Display ───────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Display")) {
        if (!running) ImGui::BeginDisabled();

        int cur_mon = running ? mi->getCurrentMonitor() : -1;
        if (ImGui::BeginMenu("Monitor")) {
            struct { const char *label; int id; } monitors[] = {
                { "Composite",          MONITOR_COMPOSITE  },
                { "GS RGB",             MONITOR_GS_RGB     },
                { "Monochrome - Green", MONITOR_MONO_GREEN },
                { "Monochrome - Amber", MONITOR_MONO_AMBER },
                { "Monochrome - White", MONITOR_MONO_WHITE },
            };
            for (auto &m : monitors) {
                bool checked = (cur_mon == m.id);
                if (ImGui::MenuItem(m.label, nullptr, checked))
                    mi->setMonitor(m.id);
            }
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Full Screen"))
            mi->displayFullScreen();

        if (!running) ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    // ── Docs ──────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Docs")) {
        if (ImGui::MenuItem("Online Documentation"))
            SDL_OpenURL("https://jawaidbazyar2.github.io/gssquared/");
        ImGui::EndMenu();
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void initMenu(SDL_Window *window)
{
    g_window = window;

    // Tear down any previous context (re-init after close/reopen)
    if (g_imgui_inited) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        g_imgui_inited  = false;
        g_grab_released = false;
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (!renderer) {
        SDL_Log("initMenu: SDL_GetRenderer returned null: %s", SDL_GetError());
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename  = nullptr; // disable imgui.ini persistence — no layout to save
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange; // don't let ImGui hide the cursor

    // Load project font at a comfortable size; fall back to built-in if not found
    std::string font_path = gs2_app_values.base_path + "fonts/OpenSans-Regular.ttf";
    ImFont *font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), MENU_FONT_SIZE);
    if (!font) {
        SDL_Log("initMenu: could not load %s, using default font", font_path.c_str());
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();

    // Make the menu bar background slightly transparent so it blends with the emulator
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_MenuBarBg]  = ImVec4(0.10f, 0.10f, 0.10f, 0.90f);
    style.Colors[ImGuiCol_Header]     = ImVec4(0.25f, 0.25f, 0.55f, 0.90f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.65f, 0.90f);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    g_imgui_inited = true;
}

void setMenuTrackingCallback(MenuIterateCallback /*callback*/, void * /*appstate*/)
{
    // ImGui is driven by the render loop; no separate tracking callback needed.
}

bool handleMenuEvent(const SDL_Event *event)
{
    if (!g_imgui_inited) return false;

    // Only forward and potentially consume SDL input events.
    // Application-level events (menu_event_type, quit, etc.) must ALWAYS pass
    // through so that actions dispatched from build_menu_bar() are not swallowed
    // while io.WantCaptureMouse is still true from the previous frame.
    const bool is_mouse_event =
        event->type == SDL_EVENT_MOUSE_MOTION      ||
        event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event->type == SDL_EVENT_MOUSE_BUTTON_UP   ||
        event->type == SDL_EVENT_MOUSE_WHEEL;

    const bool is_key_event =
        event->type == SDL_EVENT_KEY_DOWN   ||
        event->type == SDL_EVENT_KEY_UP     ||
        event->type == SDL_EVENT_TEXT_INPUT;

    if (!is_mouse_event && !is_key_event)
        return false; // not an input event — never consume it

    if (emulated_mouse_captured())
        return false; // let the emulator receive input; ImGui menu is hidden

    ImGui_ImplSDL3_ProcessEvent(event);

    ImGuiIO &io = ImGui::GetIO();
    if (is_mouse_event && io.WantCaptureMouse)   return true;
    if (is_key_event   && io.WantCaptureKeyboard) return true;
    return false;
}

void pumpMenuEvents()
{
    // No-op: ImGui is fully driven by renderMenuOverlay each frame.
}

void renderMenuOverlay(SDL_Renderer *renderer, int /*win_w*/, int /*win_h*/)
{
    if (!g_imgui_inited) return;

    if (emulated_mouse_captured())
        return;

    // Manage mouse grab: release while ImGui wants the mouse, restore when done.
    {
        ImGuiIO &io = ImGui::GetIO();
        if (io.WantCaptureMouse) {
            release_grab();
        } else {
            restore_grab();
        }
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        build_menu_bar();
        ImGui::EndMainMenuBar();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

#endif // __linux__ || __EMSCRIPTEN__
