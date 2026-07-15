# Agent instructions

## Vendored dependencies

Never edit vendored SDL libraries (or other vendored third-party code under `vendored/`).

- Do not modify files under `vendored/SDL/`, `vendored/SDL_image/`, `vendored/SDL_ttf/`, `vendored/SDL_net/`, or any other path under `vendored/`.
- If a platform limitation in SDL blocks a feature (for example save-dialog default filenames on macOS), work around it in GSSquared application code, or document the limitation — do not patch vendored sources.
- Upstream fixes belong in the upstream project, not as local edits to vendored trees.

## Build Instructions

### MacOS

#### Normal Development Cycle Build - Single File Executable

```
cmake -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build --parallel
```

or if we need debug symbols

```
cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build
cmake --build build --parallel
```

#### Build MacOS App Bundle

```
cmake -DGS2_PROGRAM_FILES=OFF  -DCMAKE_BUILD_TYPE=Release -S . -B build
cmake --build build --parallel
cmake --install build
```

## Debug-protocol smoke tests

When launching GSSquared for scripted tests over `--debug SOCKET`:

- Prefer **`c.quit()`** (protocol `QUIT`) to stop the emu. Do **not** `kill`/`SIGTERM` the process unless necessary — SDL turns those into `SDL_EVENT_QUIT`, which opens the QuitModal (“Are you sure?”) and leaves tests hung or racing a broken pipe.
- If a harness must signal-kill, start the emu with **`--no-quit-confirm`** so `SDL_EVENT_QUIT` exits without the modal / dirty-disk prompts.
- Example (IIe Enhanced / IIgs): start `./build/GSSquared --debug /tmp/gs2-….sock -p 3` (or `-p 5`), wait for the socket, then:
  `PYTHONPATH=clients/python/src python3 clients/python/examples/test_breakpoints.py /tmp/gs2-….sock 3`
  The example ends with `c.quit()`; wait for the emu process to exit (expect exit 0) instead of killing it.
- Cookbook: `Docs/gs2debug.md`. Wire protocol: `Docs/DebugProtocol.md`.
