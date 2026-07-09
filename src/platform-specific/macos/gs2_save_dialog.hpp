/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Free-standing macOS NSSavePanel for .gs2 system configs (titled window;
 *   SDL's cocoa save dialog is an untitled sheet). Existing files appear gray
 *   by design on modern macOS — that is not fixable via UTI or delegates.
 */

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

/**
 * Show a save panel for a .gs2 system config.
 * Callback semantics match SDL_ShowSaveFileDialog.
 */
void gs2_show_save_gs2_dialog(SDL_DialogFileCallback callback, void *userdata,
                              SDL_Window *window, const char *default_location);
