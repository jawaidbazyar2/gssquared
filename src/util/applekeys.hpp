#pragma once

#include <SDL3/SDL.h>

#include "util/SystemSettings.hpp"

/**
 * Host-key layout for Open Apple / Closed Apple (IIgs Command / Option).
 *
 * Menu labels and system_settings.toml values:
 *   Command = Open Apple       command_open_apple
 *   Alt = Open Apple           alt_open_apple
 *   Left Option = Open Apple   left_option_open_apple
 *
 * "Command" is the GUI key (Command on a Mac keyboard, Windows on a PC
 * keyboard). "Alt" is Option on a Mac keyboard. Left Option maps only the
 * left Alt key to Open Apple and the right Alt key to Closed Apple.
 */
enum class AppleKeyLayout {
    CommandOpenApple = 0,
    AltOpenApple = 1,
    LeftOptionOpenApple = 2,
};

inline AppleKeyLayout default_apple_key_layout() {
#if defined(__EMSCRIPTEN__)
    return AppleKeyLayout::LeftOptionOpenApple;
#elif defined(__APPLE__)
    return AppleKeyLayout::CommandOpenApple;
#else
    return AppleKeyLayout::AltOpenApple;
#endif
}

inline const char *apple_key_layout_id(AppleKeyLayout layout) {
    switch (layout) {
        case AppleKeyLayout::AltOpenApple: return "alt_open_apple";
        case AppleKeyLayout::LeftOptionOpenApple: return "left_option_open_apple";
        case AppleKeyLayout::CommandOpenApple:
        default: return "command_open_apple";
    }
}

inline const char *apple_key_layout_label(AppleKeyLayout layout) {
    switch (layout) {
        case AppleKeyLayout::AltOpenApple: return "Alt = Open Apple";
        case AppleKeyLayout::LeftOptionOpenApple: return "Left Option = Open Apple";
        case AppleKeyLayout::CommandOpenApple:
        default: return "Command = Open Apple";
    }
}

inline AppleKeyLayout apple_key_layout_from_id(const std::string &id) {
    if (id == "alt_open_apple") {
        return AppleKeyLayout::AltOpenApple;
    }
    if (id == "left_option_open_apple") {
        return AppleKeyLayout::LeftOptionOpenApple;
    }
    if (id == "command_open_apple") {
        return AppleKeyLayout::CommandOpenApple;
    }
    return default_apple_key_layout();
}

inline AppleKeyLayout current_apple_key_layout() {
    return apple_key_layout_from_id(SystemSettings::instance().apple_keys());
}

inline SDL_Keymod open_apple_mod() {
    switch (current_apple_key_layout()) {
        case AppleKeyLayout::AltOpenApple:
            return static_cast<SDL_Keymod>(SDL_KMOD_LALT | SDL_KMOD_RALT);
        case AppleKeyLayout::LeftOptionOpenApple:
            return SDL_KMOD_LALT;
        case AppleKeyLayout::CommandOpenApple:
        default:
            return static_cast<SDL_Keymod>(SDL_KMOD_LGUI | SDL_KMOD_RGUI);
    }
}

inline SDL_Keymod closed_apple_mod() {
    switch (current_apple_key_layout()) {
        case AppleKeyLayout::AltOpenApple:
            return static_cast<SDL_Keymod>(SDL_KMOD_LGUI | SDL_KMOD_RGUI);
        case AppleKeyLayout::LeftOptionOpenApple:
            return SDL_KMOD_RALT;
        case AppleKeyLayout::CommandOpenApple:
        default:
            return static_cast<SDL_Keymod>(SDL_KMOD_LALT | SDL_KMOD_RALT);
    }
}

/** True when scancode is an Open Apple or Closed Apple host key for the current layout. */
inline bool apple_modifier_scancode(SDL_Scancode scancode, bool *is_open) {
    switch (current_apple_key_layout()) {
        case AppleKeyLayout::CommandOpenApple:
            if (scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) {
                *is_open = true;
                return true;
            }
            if (scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
                *is_open = false;
                return true;
            }
            break;
        case AppleKeyLayout::AltOpenApple:
            if (scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) {
                *is_open = true;
                return true;
            }
            if (scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) {
                *is_open = false;
                return true;
            }
            break;
        case AppleKeyLayout::LeftOptionOpenApple:
            if (scancode == SDL_SCANCODE_LALT) {
                *is_open = true;
                return true;
            }
            if (scancode == SDL_SCANCODE_RALT) {
                *is_open = false;
                return true;
            }
            break;
    }
    return false;
}

#if defined(__APPLE__)
#define KEY_RESET SDLK_F12
#elif defined(__EMSCRIPTEN__)
#define KEY_RESET SDLK_F12
#elif defined(_WIN32)
#define KEY_RESET SDLK_F12
#else
#define KEY_RESET SDLK_PAUSE
#endif

#define KEYMOD_REMOVE (~(SDL_KMOD_ALT | SDL_KMOD_GUI))
