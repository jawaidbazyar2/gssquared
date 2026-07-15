#ifdef _WIN32

// Suppress min/max macros and pull in only what we need from windows.h
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include "platform-specific/menu.h"
#include "util/MenuInterface.h"

// ── Local command IDs (items not in MenuEventCode) ────────────────────────────
// Ranges already occupied: 1-4, 100-103, 200-204, 300, 400-401, 501, 600-6xx, 700-702
#define IDM_FILE_CLOSE       800
#define IDM_APP_QUIT         801
#define IDM_SETTINGS_SLEEP        802
#define IDM_SETTINGS_AUDIO_DECORR 803
#define IDM_SETTINGS_RMB_ACCEL    804
#define IDM_FILE_OPEN_CONFIG      805
#define IDM_HELP_OPEN_DOCS        900
#define IDM_HELP_DONATE           901

// DEPRECATED: see commented-out WM_ENTERMENULOOP/WM_EXITMENULOOP/WM_TIMER block below.
// #define MENU_TIMER_ID   1

// ── Module state ─────────────────────────────────────────────────────────────
static HWND  g_hwnd           = NULL;
static HMENU g_menuBar        = NULL;
static HMENU g_filePopup      = NULL;
static HMENU g_drivesMenu     = NULL;
static HMENU g_settingsPopup  = NULL;
static HMENU g_speedMenu      = NULL;
static HMENU g_controllerMenu = NULL;
static HMENU g_monitorMenu    = NULL;
static HMENU g_helpPopup      = NULL;
static std::vector<storage_key_t> g_driveKeys;

// DEPRECATED: only consumed by the commented-out timer block below.
static MenuIterateCallback g_iterateCallback = nullptr;
static void               *g_iterateAppState = nullptr;
// static UINT_PTR         g_menuTimerId     = 0;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::wstring toWide(const std::string &utf8)
{
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring out(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
    return out;
}

// ── Drive submenu (rebuilt dynamically on each open) ──────────────────────────

static void rebuildDrivesMenu()
{
    while (GetMenuItemCount(g_drivesMenu) > 0)
        DeleteMenu(g_drivesMenu, 0, MF_BYPOSITION);
    g_driveKeys.clear();

    auto drives = getMenuInterface()->getDriveList();
    if (drives.empty()) {
        AppendMenuW(g_drivesMenu, MF_STRING | MF_GRAYED, 0, L"(no drives)");
        return;
    }

    for (size_t i = 0; i < drives.size(); ++i) {
        const MenuDriveInfo &info = drives[i];
        g_driveKeys.push_back(info.key);

        std::string label = "S" + std::to_string(info.key.slot)
                          + "D" + std::to_string(info.key.drive + 1);
        if (info.is_mounted && !info.filename.empty()) {
            std::string fname = info.filename;
            size_t pos = fname.find_last_of("/\\");
            if (pos != std::string::npos) fname = fname.substr(pos + 1);
            label += ": " + fname;
            if (info.is_modified)        label += " *";
            if (info.is_write_protected) label += " [WP]";
        } else {
            label += ": (empty)";
        }

        UINT cmdId = static_cast<UINT>(MENU_DISK_TOGGLE) + static_cast<UINT>(i);
        AppendMenuW(g_drivesMenu, MF_STRING, cmdId, toWide(label).c_str());
    }
}

// ── Checkmark / enable helpers ────────────────────────────────────────────────

static void setItemCheck(HMENU menu, int pos, bool checked)
{
    CheckMenuItem(menu, static_cast<UINT>(pos),
                  MF_BYPOSITION | (checked ? MF_CHECKED : MF_UNCHECKED));
}

static void setItemEnable(HMENU menu, int pos, bool enabled)
{
    EnableMenuItem(menu, static_cast<UINT>(pos),
                   MF_BYPOSITION | (enabled ? MF_ENABLED : MF_GRAYED));
}

static UINT getItemId(HMENU menu, int pos)
{
    MENUITEMINFOW mii = {};
    mii.cbSize = sizeof(mii);
    mii.fMask  = MIIM_ID;
    GetMenuItemInfoW(menu, static_cast<UINT>(pos), TRUE, &mii);
    return mii.wID;
}

// ── Dynamic state updates (called from WM_INITMENUPOPUP) ─────────────────────

static void updatePopupState(HMENU popup)
{
    MenuInterface *mi = getMenuInterface();
    bool running = mi->isEmulationRunning();

    // ── Drives ──────────────────────────────────────────────────────────────
    if (popup == g_drivesMenu) {
        rebuildDrivesMenu();
        if (!running) {
            int n = GetMenuItemCount(g_drivesMenu);
            for (int i = 0; i < n; ++i)
                setItemEnable(g_drivesMenu, i, false);
        }
        return;
    }

    // ── File ────────────────────────────────────────────────────────────────
    if (popup == g_filePopup) {
        // pos 0 = Launch Config, pos 1 = sep, pos 2 = Drives, pos 3 = sep,
        // pos 4 = Close Emulation, pos 5 = sep, pos 6 = Quit
        setItemEnable(g_filePopup, 0, !running);  // Launch Config
        setItemEnable(g_filePopup, 2, running);   // Drives
        setItemEnable(g_filePopup, 4, running);   // Close Emulation
        setItemEnable(g_filePopup, 6, !running);  // Quit (only when not running)
        return;
    }

    // ── Speed ────────────────────────────────────────────────────────────────
    if (popup == g_speedMenu) {
        int current = mi->getCurrentSpeed(); // 1..4 matching SPEED_1_0..SPEED_14_3
        // Items are in order: 1.0(id=100), 2.8(101), 7.1(102), 14.3(103)
        // tag = id - MENU_SPEED_1_0 + 1  →  1..4
        int n = GetMenuItemCount(g_speedMenu);
        for (int i = 0; i < n; ++i) {
            int tag = static_cast<int>(getItemId(g_speedMenu, i))
                    - static_cast<int>(MENU_SPEED_1_0) + 1;
            setItemCheck(g_speedMenu,  i, tag == current);
            setItemEnable(g_speedMenu, i, running);
        }
        return;
    }

    // ── Monitor ──────────────────────────────────────────────────────────────
    if (popup == g_monitorMenu) {
        int current = mi->getCurrentMonitor(); // MONITOR_COMPOSITE(200)..
        int n = GetMenuItemCount(g_monitorMenu);
        for (int i = 0; i < n; ++i) {
            int id = static_cast<int>(getItemId(g_monitorMenu, i));
            setItemCheck(g_monitorMenu,  i, id == current);
            setItemEnable(g_monitorMenu, i, running);
        }
        return;
    }

    // ── Game Controller ──────────────────────────────────────────────────────
    if (popup == g_controllerMenu) {
        int current = mi->getCurrentControllerMode(); // 0..2
        int n = GetMenuItemCount(g_controllerMenu);
        for (int i = 0; i < n; ++i) {
            // IDs: MENU_CONTROLLER_GAMEPAD(700), MOUSE(701), JOYPORT(702)
            int mode = static_cast<int>(getItemId(g_controllerMenu, i))
                     - static_cast<int>(MENU_CONTROLLER_GAMEPAD);
            setItemCheck(g_controllerMenu,  i, mode == current);
            setItemEnable(g_controllerMenu, i, running);
        }
        return;
    }

    // ── Settings (top-level popup; contains Speed+Controller submenus + Sleep) ─
    if (popup == g_settingsPopup) {
        bool sleeping   = mi->getSleepMode();
        bool decorr_on  = mi->getAudioDecorrelation();
        bool rmb_accel  = mi->getRightMouseAccel();
        int n = GetMenuItemCount(g_settingsPopup);
        for (int i = 0; i < n; ++i) {
            UINT id = getItemId(g_settingsPopup, i);
            if (id == IDM_SETTINGS_SLEEP) {
                setItemCheck(g_settingsPopup,  i, sleeping);
                setItemEnable(g_settingsPopup, i, running);
            } else if (id == IDM_SETTINGS_AUDIO_DECORR) {
                setItemCheck(g_settingsPopup,  i, decorr_on);
                setItemEnable(g_settingsPopup, i, running);
            } else if (id == IDM_SETTINGS_RMB_ACCEL) {
                setItemCheck(g_settingsPopup,  i, rmb_accel);
                setItemEnable(g_settingsPopup, i, running);
            } else {
                // Speed and Controller submenu parent items (id == 0 for submenus)
                setItemEnable(g_settingsPopup, i, running);
            }
        }
        return;
    }

    // ── Help (always enabled regardless of emulation state) ──────────────────
    if (popup == g_helpPopup) {
        int n = GetMenuItemCount(g_helpPopup);
        for (int i = 0; i < n; ++i)
            setItemEnable(g_helpPopup, i, true);
        return;
    }

    // ── Generic fallback (Edit, Machine, Display popups) ─────────────────────
    // Enable/disable every non-separator item based on running state.
    int n = GetMenuItemCount(popup);
    for (int i = 0; i < n; ++i) {
        MENUITEMINFOW mii = {};
        mii.cbSize = sizeof(mii);
        mii.fMask  = MIIM_FTYPE;
        GetMenuItemInfoW(popup, static_cast<UINT>(i), TRUE, &mii);
        if (mii.fType & MFT_SEPARATOR) continue;
        setItemEnable(popup, i, running);
    }
}

// ── WM_COMMAND dispatcher ─────────────────────────────────────────────────────

static void dispatchCommand(UINT id)
{
    MenuInterface *mi = getMenuInterface();

    switch (id) {
    // File
    case IDM_FILE_OPEN_CONFIG:
        mi->openSystemConfig();
        return;
    case IDM_FILE_CLOSE:
        if (SDL_EventEnabled(SDL_EVENT_QUIT)) {
            SDL_Event ev = {};
            ev.type = SDL_EVENT_QUIT;
            SDL_PushEvent(&ev);
        }
        return;
    case IDM_APP_QUIT:
        PostQuitMessage(0);
        return;

    // Machine
    case MENU_MACHINE_RESET:         mi->machineReset();        return;
    case MENU_MACHINE_RESTART:       mi->machineRestart();      return;
    case MENU_MACHINE_PAUSE_RESUME:  mi->machinePauseResume();  return;
    case MENU_MACHINE_CAPTURE_MOUSE: mi->machineCaptureMouse(); return;

    // Speed
    case MENU_SPEED_1_0:  mi->setSpeed(SPEED_1_0);  return;
    case MENU_SPEED_2_8:  mi->setSpeed(SPEED_2_8);  return;
    case MENU_SPEED_7_1:  mi->setSpeed(SPEED_7_1);  return;
    case MENU_SPEED_14_3: mi->setSpeed(SPEED_14_3); return;

    // Monitor
    case MENU_MONITOR_COMPOSITE:  mi->setMonitor(MONITOR_COMPOSITE);  return;
    case MENU_MONITOR_GS_RGB:     mi->setMonitor(MONITOR_GS_RGB);     return;
    case MENU_MONITOR_MONO_GREEN: mi->setMonitor(MONITOR_MONO_GREEN); return;
    case MENU_MONITOR_MONO_AMBER: mi->setMonitor(MONITOR_MONO_AMBER); return;
    case MENU_MONITOR_MONO_WHITE: mi->setMonitor(MONITOR_MONO_WHITE); return;

    // Display
    case MENU_DISPLAY_FULLSCREEN: mi->displayFullScreen(); return;

    // Edit
    case MENU_EDIT_COPY_SCREEN: mi->editCopyScreen(); return;
    case MENU_EDIT_PASTE_TEXT:  mi->editPasteText();  return;

    // Settings
    case IDM_SETTINGS_SLEEP:        mi->toggleSleepMode();         return;
    case IDM_SETTINGS_AUDIO_DECORR: mi->toggleAudioDecorrelation(); return;
    case IDM_SETTINGS_RMB_ACCEL:    mi->toggleRightMouseAccel();    return;

    // Help
    case IDM_HELP_OPEN_DOCS:
        SDL_OpenURL("https://jawaidbazyar2.github.io/gssquared/");
        return;
    case IDM_HELP_DONATE:
        SDL_OpenURL("https://gssquared.net/support");
        return;

    // Game Controller
    case MENU_CONTROLLER_GAMEPAD: mi->setControllerMode(0); return;
    case MENU_CONTROLLER_MOUSE:   mi->setControllerMode(1); return;
    case MENU_CONTROLLER_JOYPORT: mi->setControllerMode(2); return;

    default:
        // Drive toggle: IDs [MENU_DISK_TOGGLE, MENU_DISK_TOGGLE + N)
        if (id >= static_cast<UINT>(MENU_DISK_TOGGLE)) {
            size_t idx = static_cast<size_t>(id - MENU_DISK_TOGGLE);
            if (idx < g_driveKeys.size())
                mi->diskToggle(g_driveKeys[idx]);
        }
        return;
    }
}

// ── SDL Windows message hook ──────────────────────────────────────────────────

static bool SDLCALL windowsMessageHook(void * /*userdata*/, MSG *msg)
{
    if (!g_hwnd || msg->hwnd != g_hwnd)
        return true;

    switch (msg->message) {
    case WM_COMMAND:
        // HIWORD == 0: menu command; HIWORD == 1: accelerator
        if (HIWORD(msg->wParam) == 0 || HIWORD(msg->wParam) == 1)
            dispatchCommand(LOWORD(msg->wParam));
        break;

    case WM_INITMENUPOPUP:
        updatePopupState(reinterpret_cast<HMENU>(msg->wParam));
        break;

    // DEPRECATED: SDL3 already handles modal-loop keep-alive on Windows.
    // WIN_WindowProc intercepts WM_ENTERMENULOOP itself, installs a timer keyed
    // to SDL_IterateMainCallbacks, and calls SDL_OnWindowLiveResizeUpdate on
    // every tick — exactly what this code was trying to do.  Left here for
    // reference until confirmed safe to delete.
    //
    // case WM_ENTERMENULOOP:
    //     if (g_iterateCallback && g_menuTimerId == 0)
    //         g_menuTimerId = SetTimer(g_hwnd, MENU_TIMER_ID, 16, NULL);
    //     break;
    //
    // case WM_EXITMENULOOP:
    //     if (g_menuTimerId != 0) {
    //         KillTimer(g_hwnd, MENU_TIMER_ID);
    //         g_menuTimerId = 0;
    //     }
    //     break;
    //
    // case WM_TIMER:
    //     if (msg->wParam == MENU_TIMER_ID && g_iterateCallback)
    //         g_iterateCallback(g_iterateAppState);
    //     break;

    default:
        break;
    }

    return true; // always let SDL continue processing
}

// ── Menu bar construction ─────────────────────────────────────────────────────

static void setupMenus()
{
    g_menuBar = CreateMenu();

    // ── File ────────────────────────────────────────────────────────────────
    g_filePopup  = CreatePopupMenu();
    g_drivesMenu = CreatePopupMenu();
    // pos 0
    AppendMenuW(g_filePopup, MF_STRING, IDM_FILE_OPEN_CONFIG, L"Launch Config...");
    // pos 1
    AppendMenuW(g_filePopup, MF_SEPARATOR, 0, nullptr);
    // pos 2
    AppendMenuW(g_filePopup, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_drivesMenu), L"Drives");
    // pos 3
    AppendMenuW(g_filePopup, MF_SEPARATOR, 0, nullptr);
    // pos 4
    AppendMenuW(g_filePopup, MF_STRING, IDM_FILE_CLOSE, L"Close Emulation");
    // pos 5
    AppendMenuW(g_filePopup, MF_SEPARATOR, 0, nullptr);
    // pos 6
    AppendMenuW(g_filePopup, MF_STRING, IDM_APP_QUIT, L"Quit");
    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_filePopup), L"File");

    // ── Edit ────────────────────────────────────────────────────────────────
    HMENU editPopup = CreatePopupMenu();
    AppendMenuW(editPopup, MF_STRING, MENU_EDIT_COPY_SCREEN, L"Copy Screen");
    AppendMenuW(editPopup, MF_STRING, MENU_EDIT_PASTE_TEXT,  L"Paste Text");
    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(editPopup), L"Edit");

    // ── Machine ─────────────────────────────────────────────────────────────
    HMENU machinePopup = CreatePopupMenu();
    AppendMenuW(machinePopup, MF_STRING, MENU_MACHINE_RESET,         L"Reset");
    AppendMenuW(machinePopup, MF_STRING, MENU_MACHINE_RESTART,       L"Restart");
    AppendMenuW(machinePopup, MF_STRING, MENU_MACHINE_PAUSE_RESUME,  L"Pause / Resume");
    AppendMenuW(machinePopup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(machinePopup, MF_STRING, MENU_MACHINE_CAPTURE_MOUSE, L"Capture Mouse");
    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(machinePopup), L"Machine");

    // ── Settings ────────────────────────────────────────────────────────────
    g_settingsPopup  = CreatePopupMenu();
    g_speedMenu      = CreatePopupMenu();
    g_controllerMenu = CreatePopupMenu();

    AppendMenuW(g_speedMenu, MF_STRING, MENU_SPEED_1_0,  L"1.0 MHz");
    AppendMenuW(g_speedMenu, MF_STRING, MENU_SPEED_2_8,  L"2.8 MHz");
    AppendMenuW(g_speedMenu, MF_STRING, MENU_SPEED_7_1,  L"7.1 MHz");
    AppendMenuW(g_speedMenu, MF_STRING, MENU_SPEED_14_3, L"14.3 MHz");
    AppendMenuW(g_settingsPopup, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_speedMenu), L"Speed");

    AppendMenuW(g_controllerMenu, MF_STRING, MENU_CONTROLLER_GAMEPAD, L"Joystick - Gamepad");
    AppendMenuW(g_controllerMenu, MF_STRING, MENU_CONTROLLER_MOUSE,   L"Joystick - Mouse");
    AppendMenuW(g_controllerMenu, MF_STRING, MENU_CONTROLLER_JOYPORT, L"Sirius / Atari Joyport");
    AppendMenuW(g_settingsPopup, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_controllerMenu), L"Game Controller");

    AppendMenuW(g_settingsPopup, MF_STRING, IDM_SETTINGS_SLEEP,        L"Sleep / Busy Wait");
    AppendMenuW(g_settingsPopup, MF_STRING, IDM_SETTINGS_AUDIO_DECORR, L"Mono Helper");
    AppendMenuW(g_settingsPopup, MF_STRING, IDM_SETTINGS_RMB_ACCEL,    L"Right Mouse Button Accelerate");

    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_settingsPopup), L"Settings");

    // ── Display ─────────────────────────────────────────────────────────────
    HMENU displayPopup = CreatePopupMenu();
    g_monitorMenu      = CreatePopupMenu();

    AppendMenuW(g_monitorMenu, MF_STRING, MENU_MONITOR_COMPOSITE,  L"Composite");
    AppendMenuW(g_monitorMenu, MF_STRING, MENU_MONITOR_GS_RGB,     L"GS RGB");
    AppendMenuW(g_monitorMenu, MF_STRING, MENU_MONITOR_MONO_GREEN, L"Monochrome - Green");
    AppendMenuW(g_monitorMenu, MF_STRING, MENU_MONITOR_MONO_AMBER, L"Monochrome - Amber");
    AppendMenuW(g_monitorMenu, MF_STRING, MENU_MONITOR_MONO_WHITE, L"Monochrome - White");
    AppendMenuW(displayPopup, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_monitorMenu), L"Monitor");

    AppendMenuW(displayPopup, MF_STRING, MENU_DISPLAY_FULLSCREEN, L"Full Screen");
    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(displayPopup), L"Display");

    // ── Docs ─────────────────────────────────────────────────────────────────
    g_helpPopup = CreatePopupMenu();
    AppendMenuW(g_helpPopup, MF_STRING, IDM_HELP_OPEN_DOCS, L"Online Documentation");
    AppendMenuW(g_helpPopup, MF_STRING, IDM_HELP_DONATE, L"Donate");
    AppendMenuW(g_menuBar, MF_STRING | MF_POPUP,
                reinterpret_cast<UINT_PTR>(g_helpPopup), L"Docs");

    SetMenu(g_hwnd, g_menuBar);
}

// ── Public API ────────────────────────────────────────────────────────────────

void initMenu(SDL_Window *window)
{
    HWND newHwnd = (HWND)SDL_GetPointerProperty(
        SDL_GetWindowProperties(window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        NULL);

    if (!newHwnd)
        return;

    if (newHwnd == g_hwnd)
        return; // same window, menu already attached

    // New window (e.g. second VM after closing the first).
    // The old window's destruction already freed its attached menu via
    // DestroyWindow → DestroyMenu, so just null out the stale handles.
    g_hwnd           = newHwnd;
    g_menuBar        = NULL;
    g_filePopup      = NULL;
    g_drivesMenu     = NULL;
    g_settingsPopup  = NULL;
    g_speedMenu      = NULL;
    g_controllerMenu = NULL;
    g_monitorMenu    = NULL;
    g_helpPopup      = NULL;
    g_driveKeys.clear();

    setupMenus();

    // Register the hook once; subsequent calls are harmless — SDL keeps
    // only the most-recently-set hook, which stays the same function.
    SDL_SetWindowsMessageHook(windowsMessageHook, nullptr);
}

void setMenuTrackingCallback(MenuIterateCallback callback, void *appstate)
{
    g_iterateCallback = callback;
    g_iterateAppState = appstate;
}

#endif // _WIN32
