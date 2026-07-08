#pragma once

#include "devices/adb/keygloo_state.hpp"
#include "gs2.hpp"

// AGENT_MOUSEID and AGENT_USER_SET_MOUSE_MODE — the agent-side wire
// constants used to mark injected SDL events and to marshal mode-set
// requests onto the main thread — are defined in agent/Protocol.hpp.
// Files that need them (keygloo.cpp, gs2.cpp) include Protocol.hpp
// directly; we don't pull it in here to keep the keygloo header light.
//
// MouseMode and the keygloo_state_t mouse_mode field live in
// keygloo_state.hpp (included above).

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
