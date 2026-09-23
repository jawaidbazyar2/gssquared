// Portable Dear ImGui menu shared by the Linux and Emscripten (web) builds.
// It only uses ImGui + the SDL3 renderer backend; nothing GTK/X11-specific.
#if defined(__linux__) || defined(__EMSCRIPTEN__)

#include <SDL3/SDL.h>

#include <imgui.h>
#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/backends/imgui_impl_sdlrenderer3.h"

#include "platform-specific/menu.h"
#include "util/CheckForUpdates.hpp"
#include "util/MenuInterface.h"
#include "gs2.hpp"

static constexpr float MENU_FONT_SIZE = 20.0f;

// Dropdown inset. The bar window forces WindowPadding to zero, and that style
// is still on the stack when a popup opens, so each menu pushes its own.
// Top stays flush with the bar; left, right, and bottom get room.
static constexpr float kMenuPadX      = 16.0f;
static constexpr float kMenuPadBottom = 10.0f;

static bool begin_menu(const char *label, bool enabled = true)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kMenuPadX, 0.0f));
    const bool open = ImGui::BeginMenu(label, enabled);
    if (!open)
        ImGui::PopStyleVar();
    return open;
}

static void end_menu()
{
    ImGui::Dummy(ImVec2(0.0f, kMenuPadBottom));
    ImGui::EndMenu();
    ImGui::PopStyleVar();
}

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

static void render_new_disk_image_menu(MenuInterface *mi)
{
    if (begin_menu("New Disk Image")) {
        if (ImGui::MenuItem("5.25 Unformatted"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_525_UNFMT);
        if (ImGui::MenuItem("5.25 Formatted DOS 3.3"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_525_DOS33);
        if (ImGui::MenuItem("5.25 Formatted ProDOS"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_525_PRODOS);
        if (ImGui::MenuItem("3.5 Formatted ProDOS"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_35_PRODOS);
        if (ImGui::MenuItem("32M HD Unformatted"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_32M_HD);
        if (ImGui::MenuItem("32M HD Formatted ProDOS"))
            mi->newDiskImage(MENU_FILE_NEW_DISK_32M_PRODOS);
        end_menu();
    }
}

static void build_menu_bar()
{
    MenuInterface *mi      = getMenuInterface();
    bool           running = mi->isEmulationRunning();

    // ── File ─────────────────────────────────────────────────────────────────
    if (begin_menu("File")) {
        if (!running) {
            if (ImGui::MenuItem("Launch Config...")) {
                mi->openSystemConfig();
            }
            render_new_disk_image_menu(mi);
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) {
                SDL_Event ev = {};
                ev.type = SDL_EVENT_QUIT;
                SDL_PushEvent(&ev);
            }
        } else {
            render_new_disk_image_menu(mi);
            if (begin_menu("Drives")) {
                render_drives_menu();
                end_menu();
            }
            {
                bool drivers_on = mi->getMountDrivers();
                bool can_mount  = mi->hasBazFast();
                if (!can_mount) ImGui::BeginDisabled();
                if (ImGui::MenuItem("Mount Drivers", nullptr, drivers_on)) {
                    mi->toggleMountDrivers();
                }
                if (!can_mount) ImGui::EndDisabled();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Save Screenshot", "Shift+PrintScreen")) {
                mi->fileSaveScreenshot();
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
        }
        end_menu();
    }

    // ── Edit ─────────────────────────────────────────────────────────────────
    if (begin_menu("Edit")) {
        if (ImGui::MenuItem("Copy Screen"))  mi->editCopyScreen();
        if (ImGui::MenuItem("Paste Text"))   mi->editPasteText();
        end_menu();
    }

    // ── Machine ───────────────────────────────────────────────────────────────
    if (begin_menu("Machine")) {
        if (!running) ImGui::BeginDisabled();
        if (ImGui::MenuItem("Reset"))         mi->machineReset();
        if (ImGui::MenuItem("Restart"))       mi->machineRestart();
        if (ImGui::MenuItem("Pause / Resume")) mi->machinePauseResume();
        ImGui::Separator();
        if (ImGui::MenuItem("Capture Mouse")) mi->machineCaptureMouse();
        if (!running) ImGui::EndDisabled();
        end_menu();
    }

    // ── Settings ──────────────────────────────────────────────────────────────
    if (begin_menu("Settings")) {
        if (!running) ImGui::BeginDisabled();

        // Speed submenu
        int cur_speed = running ? mi->getCurrentSpeed() : 0;
        if (begin_menu("Speed")) {
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
            end_menu();
        }

        // Game Controller submenu
        int cur_ctrl = running ? mi->getCurrentControllerMode() : -1;
        if (begin_menu("Game Controller")) {
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
            ImGui::Separator();
            bool joyport_on = (cur_ctrl == 2);
            if (begin_menu("Joyport Controller Select", joyport_on)) {
                int cur_sel = mi->getJoyportSelect();
                struct { const char *label; int select; } selects[] = {
                    { "Left",   0 },
                    { "Center", 1 },
                    { "Right",  2 },
                };
                for (auto &s : selects) {
                    bool checked = (cur_sel == s.select);
                    if (ImGui::MenuItem(s.label, nullptr, checked))
                        mi->setJoyportSelect(s.select);
                }
                end_menu();
            }
            ImGui::Separator();
            bool absent_disconnected = mi->getDisconnectedWhenNoGamepad();
            if (ImGui::MenuItem("Disconnected When No Gamepad", nullptr, absent_disconnected))
                mi->toggleDisconnectedWhenNoGamepad();
            end_menu();
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

        end_menu();
    }

    // ── Display ───────────────────────────────────────────────────────────────
    if (begin_menu("Display")) {
        if (!running) ImGui::BeginDisabled();

        int cur_mon = running ? mi->getCurrentMonitor() : -1;
        if (begin_menu("Monitor")) {
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
            end_menu();
        }

        if (begin_menu("HUD")) {
            bool stats_on = mi->getHudStats();
            if (ImGui::MenuItem("Stats", nullptr, stats_on))
                mi->toggleHudStats();
            bool drives_on = mi->getHudDrives();
            if (ImGui::MenuItem("Drives", nullptr, drives_on))
                mi->toggleHudDrives();
            end_menu();
        }

        if (ImGui::MenuItem("Full Screen"))
            mi->displayFullScreen();

        {
            bool ss_on = mi->getSsTextMode();
            bool has_ss = mi->hasSecondSight();
            if (!has_ss) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Second Sight Text", nullptr, ss_on))
                mi->toggleSsTextMode();
            if (!has_ss) ImGui::EndDisabled();
        }

        {
            bool crt_on = mi->getCrtShader();
            bool crt_avail = mi->getCrtShaderAvailable();
            if (!crt_avail) ImGui::BeginDisabled();
            if (ImGui::MenuItem("CRT Shader", "F7", crt_on))
                mi->toggleCrtShader();
            if (!crt_avail) ImGui::EndDisabled();
        }

        if (!running) ImGui::EndDisabled();
        end_menu();
    }

    // ── Docs ──────────────────────────────────────────────────────────────────
    if (begin_menu("Docs")) {
        if (ImGui::MenuItem("Check For Updates"))
            openCheckForUpdates();
        if (ImGui::MenuItem("Online Documentation"))
            SDL_OpenURL("https://jawaidbazyar2.github.io/gssquared/");
        if (ImGui::MenuItem("Donate"))
            SDL_OpenURL("https://gssquared.net/support");
        end_menu();
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

bool menuNeedsFrame()
{
    if (!g_imgui_inited) return false;
    ImGuiIO &io = ImGui::GetIO();
    // WantCapture* is set at end of the previous NewFrame. While either is set,
    // handleMenuEvent swallows input that would otherwise dirty SelectSystem /
    // EditSystem — so those phases must keep calling renderMenuOverlay or ImGui
    // (and the rest of the UI) freezes.
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}

static bool g_menu_frame_open = false;

static void render_menu_overlay_impl(SDL_Renderer *renderer, const SDL_FRect *content, bool draw)
{
    if (!g_imgui_inited) return;

    if (emulated_mouse_captured())
        return;

    // A DOM click can re-enter while NewFrame is still building the bar
    // (the file input's own click). Don't start a second ImGui frame.
    if (g_menu_frame_open)
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

    g_menu_frame_open = true;
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Sit the bar immediately above the letterboxed dest (same width), not at
    // the raw window top and not over the dest's widgets. SDL_RenderPresent
    // paints LETTERBOX bars black, so a window-top bar — or a dropdown that
    // hangs into a side bar — gets erased on many Emscripten backends. Flush
    // against the dest keeps open menus over the content; when there is no
    // room above (wide canvas) the bar shares the dest top and the selector /
    // editor layouts leave menuBarHeight() of room underneath.
    float bar_x = 0.0f;
    float bar_y = 0.0f;
    float bar_w = ImGui::GetIO().DisplaySize.x;
    const float bar_h = ImGui::GetFrameHeight();
    if (content && content->w > 0.0f) {
        bar_x = content->x;
        bar_w = content->w;
        bar_y = (content->y >= bar_h) ? (content->y - bar_h) : content->y;
    }

    ImGui::SetNextWindowPos(ImVec2(bar_x, bar_y));
    ImGui::SetNextWindowSize(ImVec2(bar_w, bar_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
    const ImGuiWindowFlags bar_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("##GS2MenuBar", nullptr, bar_flags)) {
        if (ImGui::BeginMenuBar()) {
            build_menu_bar();
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(4);

    ImGui::Render();
    g_menu_frame_open = false;
    if (draw && renderer)
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void renderMenuOverlay(SDL_Renderer *renderer, const SDL_FRect *content)
{
    render_menu_overlay_impl(renderer, content, true);
}

void advanceMenuFrameForGesture(SDL_Renderer *renderer, const SDL_FRect *content)
{
    render_menu_overlay_impl(renderer, content, false);
}

#endif // __linux__ || __EMSCRIPTEN__
