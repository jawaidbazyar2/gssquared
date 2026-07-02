#pragma once

#include "computer.hpp"

#include "mmus/mmu_ii.hpp"
#include "devices/adb/ADB_Micro.hpp"
#include "util/InterruptController.hpp"
#include "util/ResetController.hpp"

// AGENT_MOUSEID and AGENT_USER_SET_MOUSE_MODE — the agent-side wire
// constants used to mark injected SDL events and to marshal mode-set
// requests onto the main thread — are defined in agent/Protocol.hpp.
// Files that need them (keygloo.cpp, gs2.cpp) include Protocol.hpp
// directly; we don't pull it in here to keep the keygloo header light.

// Mouse-input source mode. Round-robins via F1 / middle-click / a menu
// item / the agent's TAG_SET_MOUSE_MODE wire packet.
//
//   FOLLOW_HOST  - real host SDL mouse events drive the IIgs cursor;
//                  agent-injected events also work. (default)
//   CAPTURE      - host cursor is locked inside the SDL window via
//                  SDL_SetWindowRelativeMouseMode; otherwise like
//                  FOLLOW_HOST.
//   DISABLED     - real host mouse events are dropped at the keygloo
//                  layer; only events tagged with AGENT_MOUSEID reach
//                  the IIgs. Use this for automated agent-driven runs
//                  so the operator's hand on the host mouse can't
//                  contaminate what the IIgs sees.
enum MouseMode : int {
    MOUSE_MODE_FOLLOW_HOST = 0,
    MOUSE_MODE_CAPTURE     = 1,
    MOUSE_MODE_DISABLED    = 2,
    MOUSE_MODE_COUNT       = 3,
};

struct keygloo_state_t {
    KeyGloo *kg = nullptr;
    MMU_II *mmu = nullptr;
    computer_t *computer = nullptr;
    InterruptController *irq_control = nullptr;
    ResetController *reset_control = nullptr;
    MouseMode mouse_mode = MOUSE_MODE_FOLLOW_HOST;
};

void init_slot_keygloo(computer_t *computer, SlotType_t slot);

// Mode plumbing — all callers run on the main thread. Look up
// keygloo_state_t via computer->get_module_state(MODULE_KEYGLOO).
//
// keygloo_set_mouse_mode: set a specific mode and apply side-effects
//   (toggling SDL_SetWindowRelativeMouseMode for CAPTURE).
// keygloo_cycle_mouse_mode: advance to the next mode in round-robin
//   order. Used by F1, middle-click, and the menu item.
// keygloo_get_mouse_mode: current mode for the menu UI.
// keygloo_mouse_mode_name: short human label ("follow host", etc.).
void keygloo_set_mouse_mode(computer_t *computer, MouseMode mode);
void keygloo_cycle_mouse_mode(computer_t *computer);
MouseMode keygloo_get_mouse_mode(computer_t *computer);
const char *keygloo_mouse_mode_name(MouseMode mode);