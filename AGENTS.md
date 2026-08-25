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

### Windows (MSYS2 / MinGW64)

Windows development uses **MSYS2** with the MinGW64 toolchain (not a native PowerShell/`cmd` environment).

- Open an MSYS2 bash shell (or invoke `C:\msys64\usr\bin\bash.exe`). Home is `/home/<user>` (`C:\msys64\home\<user>`), not `%USERPROFILE%`.
- **Always `source ~/.profile` first** so compiler, cmake, and other MinGW tools are on `PATH` (`/c/msys64/mingw64/bin`). Without that, `g++`/`clang++`/`cmake` may be missing or the wrong binaries.
- From a non-login shell (including many agent terminals), use:

```
source ~/.profile
```

or start bash as a login shell (`bash -lc '…'`), which reads `~/.profile` automatically.

```
source ~/.profile
cmake -G "MinGW Makefiles" -DGS2_PROGRAM_FILES=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build -S .
cmake --build build --parallel
```

## Debug-protocol smoke tests

When launching GSSquared for scripted tests over `--debug SOCKET`:

- Prefer **`c.quit()`** (protocol `QUIT`) to stop the emu. Do **not** `kill`/`SIGTERM` the process unless necessary — SDL turns those into `SDL_EVENT_QUIT`, which opens the QuitModal (“Are you sure?”) and leaves tests hung or racing a broken pipe.
- If a harness must signal-kill, start the emu with **`--no-quit-confirm`** so `SDL_EVENT_QUIT` exits without the modal / dirty-disk prompts.
- Example (IIe Enhanced / IIgs): start `./build/GSSquared --debug /tmp/gs2-….sock -p 3` (or `-p 5`), wait for the socket, then:
  `PYTHONPATH=clients/python/src python3 clients/python/examples/test_breakpoints.py /tmp/gs2-….sock 3`
  The example ends with `c.quit()`; wait for the emu process to exit (expect exit 0) instead of killing it.
- Cookbook: `Docs/gs2debug.md`. Wire protocol: `Docs/DebugProtocol.md`.

## Disk Images

To inspect Apple II disk images and their contents, use CiderPress2:

```
cp2='~/src/cp2_1.0.5_osx-x64_sc/cp2'
```

## Release packages (macOS + Windows + Linux)

Native package builds are orchestrated from this Mac by `scripts/release/gs2-release`. Copy `scripts/release/hosts.toml.example` to `scripts/release/hosts.toml` and fill in SSH hosts. Run `unpushed` before `sync` so no host is sitting on unpushed commits. `sync` creates a detached worktree at `<repo>-release` (e.g. `~/src/gssquared-release`) from `origin/main`; package builds run there, not in the everyday clone. Agent workflow: `.cursor/skills/gs2-release/SKILL.md`.
