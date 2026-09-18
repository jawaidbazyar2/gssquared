/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Free-standing macOS NSSavePanel (titled window; SDL's cocoa save dialog
 *   is an untitled sheet). Existing files appear gray by design on modern
 *   macOS — that is not fixable via UTI or delegates.
 */

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

/**
 * Show a titled NSSavePanel. Callback semantics match SDL_ShowSaveFileDialog.
 * default_location is split into directory + name field. fallback_name is used
 * when default_location has no filename component.
 */
void gs2_show_save_file_dialog(SDL_DialogFileCallback callback, void *userdata,
                               SDL_Window *window, const char *default_location,
                               const char *title, const char *message,
                               const char *fallback_name);

/**
 * Show a save panel for a .gs2 system config.
 * Callback semantics match SDL_ShowSaveFileDialog.
 */
void gs2_show_save_gs2_dialog(SDL_DialogFileCallback callback, void *userdata,
                              SDL_Window *window, const char *default_location);
