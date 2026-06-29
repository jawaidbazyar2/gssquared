# Emscripten (WebAssembly) Build

GSSquared can be compiled to WebAssembly with Emscripten and run in a browser.
The same top-level `CMakeLists.txt` is used for native and web builds; the web
build is selected entirely by invoking CMake through `emcmake`, which injects
Emscripten's toolchain (defining `EMSCRIPTEN`, setting `CMAKE_SYSTEM_NAME` to
`Emscripten`, and leaving `APPLE` unset). Use a separate build directory so the
native and web builds coexist.

## Prerequisites

Install Emscripten. On macOS via Homebrew:

```bash
brew install emscripten
```

Homebrew's package puts its cache directory under a non-writable location, so
set `EM_CACHE` to a writable path before building (any directory works):

```bash
export EM_CACHE="$HOME/.emscripten_cache"   # or e.g. "$PWD/.emcache"
mkdir -p "$EM_CACHE"
```

(If you use the official `emsdk` instead, `source /path/to/emsdk/emsdk_env.sh`
and you can skip the `EM_CACHE` step.)

## Configure and build

```bash
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web --target GSSquared -j
```

This produces, in `build-web/`:

- `GSSquared.html`  – the page (generated from `assets/web/shell.html`)
- `GSSquared.js`    – loader/runtime glue
- `GSSquared.wasm`  – the compiled emulator
- `GSSquared.data`  – the preloaded resource bundle (ROMs, fonts, images)
- `GSSquared.worker.js` (and friends) – pthread workers

## Run it

The build uses pthreads, which requires `SharedArrayBuffer`. Browsers only
expose that when the page is served with cross-origin isolation headers:

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

A helper dev server that sets these is included:

```bash
python3 assets/web/serve.py 8000 build-web
# then open http://localhost:8000/GSSquared.html
```

Opening `GSSquared.html` directly from `file://` will **not** work (no headers,
and WASM fetch is blocked). Always serve over HTTP with the headers above.

## Using disk images

Two ways to mount media in the browser:

- **Drag and drop** a disk image (`.dsk`, `.do`, `.po`, `.woz`, `.hdv`, `.2mg`,
  `.img`) onto the canvas, dropping it on a drive button in the drives panel.
  The file is read into the in-memory filesystem and mounted.
- **File menu -> Drives**, or clicking a drive button, opens a browser file
  picker (replacing the native open dialog, which has no web backend).

Mounted images live in the in-memory filesystem and are not persisted across
page reloads.

## Notes / limitations

- **Audio** starts only after the first user gesture (browser autoplay policy).
  The shell shows a "Click to start" overlay for this.
- **Networking / modem**: SDL_net has no Emscripten backend (its
  interface-enumeration code is a hard `#error` for unknown platforms) and the
  browser sandbox blocks raw TCP anyway. The web build therefore does **not**
  build or link SDL_net, and the SCC channel-B modem (`ModemDevice`, the only
  SDL_net user) is compiled out under `__EMSCRIPTEN__` in
  `src/devices/scc8530/scc8530.cpp` (channel B falls back to a file device). No
  vendored library source is modified.
- **Drag-and-drop temp dir**: SDL's Emscripten backend runs an unguarded
  `FS.mkdir("/tmp/filedrop")` during `SDL_CreateWindow`; if that directory
  already exists (or `/tmp` is missing) it throws and aborts init. Rather than
  patch vendored SDL, `video_system_t` ensures `/tmp` exists and clears any
  stale `/tmp/filedrop` before creating each window (see
  `gs2_web_prepare_filedrop_dir()` in `src/videosystem.cpp`).
- **Keyboard focus**: SDL only treats the window as keyboard-focused when the
  canvas is `document.activeElement`, so input dies after Alt-Tab. The shell
  (`assets/web/shell.html`) re-focuses the canvas on window `focus`/`click`.
  When there is no keyboard focus, SDL still delivers key events but with
  `windowID == 0`. Emscripten supports only one window, so the debugger's
  second `SDL_CreateWindow()` returns NULL and its `window_id` is also 0 —
  `debug_window_t::handle_event()` therefore guards on a valid/nonzero
  `window_id` so the (nonexistent) debug window doesn't swallow every keystroke
  meant for the emulator.
- **Memory**: `INITIAL_MEMORY` is set to 256 MB with `ALLOW_MEMORY_GROWTH`. If a
  large machine (e.g. IIgs) fails to start with an out-of-memory error, raise
  `INITIAL_MEMORY` in the `if(EMSCRIPTEN)` block of `CMakeLists.txt`.
- The auxiliary `apps/` developer tools are not built for the web target.
