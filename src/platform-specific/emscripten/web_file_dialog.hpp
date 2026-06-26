#pragma once

#include <SDL3/SDL.h>

#if defined(__EMSCRIPTEN__)

// Browser replacement for SDL_ShowOpenFileDialog (which has no Emscripten
// backend). Pops a hidden HTML <input type="file">, reads the chosen file into
// the WASM filesystem under /uploads/, then invokes `callback` with a filelist
// of { "/uploads/<name>", NULL } — matching SDL_DialogFileCallback semantics so
// existing callbacks (e.g. OSD's file_dialog_callback) can be reused verbatim.
//
// `accept` is an optional comma-separated list of extensions for the input's
// accept attribute (e.g. ".dsk,.po,.woz"); pass nullptr to accept any file.
//
// Only one dialog may be outstanding at a time; a new call supersedes a pending
// one. `userdata` is owned by the caller (same contract as SDL).
void web_open_file_dialog(SDL_DialogFileCallback callback, void *userdata, const char *accept);

#endif // __EMSCRIPTEN__
