#pragma once

class KeyGloo;
class MMU_II;
class computer_t;
class InterruptController;
class ResetController;

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
