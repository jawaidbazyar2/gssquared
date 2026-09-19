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
- **Always `source ~/.profile` first** (inside bash) so compiler, cmake, and other MinGW tools are on `PATH` (`/c/msys64/mingw64/bin`). Without that, `g++`/`clang++`/`cmake` may be missing or the wrong binaries.
- Never, ever ever use PowerShell for anything. I consider it to be garbage. This project's entire build system and ethos is **MSYS2** **MINGW64** **bash**.
- Do not use PowerShell to run MSYS2. Do not set `$env:MSYSTEM`, do not chain with PowerShell `;` / `&&`, do not call `source ~/.profile` from PowerShell (it is a bash builtin and does nothing there).

Cursor/VS Code agent terminals on Windows still spawn PowerShell as the *host*. You cannot make that host be bash. Force MSYS2 by making the **command itself** an MSYS2 process: `env.exe` then `bash.exe`. That sets MinGW64 on the bash process *before* `--login` runs `/etc/profile`. Wrapping bash in `$env:MSYSTEM='MINGW64'; bash.exe ...` is still PowerShell, and PowerShell will eat nested quotes (e.g. `-G "MinGW Makefiles"` becomes generator `MinGW`, and you may get Windows CMake from Visual Studio instead of `/mingw64/bin/cmake`).

Skip the wrap only when `uname -s` already reports `MINGW64` or `MSYS`.

Every agent command on Windows:

```
C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\msys64\usr\bin\bash.exe --login -lc '<command>'
```

- `MSYSTEM=MINGW64` — must be set on the bash process before `--login`, so the login profile puts `/mingw64/bin` first.
- `CHERE_INVOKING=1` — keep the current working directory (otherwise the login shell starts in `/home/<user>`).
- Put the bash snippet in **single** quotes. Avoid nested `"` inside `-lc` when the agent host is PowerShell; it splits arguments. For cmake's generator, write `MinGW\ Makefiles` (backslash-escaped space) or put the command in a `.sh` and `bash` that file.
- Inside the bash command, `source ~/.profile` if tools are still missing from `PATH`.

```
C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\msys64\usr\bin\bash.exe --login -lc 'source ~/.profile; cmake -G MinGW\ Makefiles -DGS2_PROGRAM_FILES=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -B build -S .'
C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\msys64\usr\bin\bash.exe --login -lc 'source ~/.profile; cmake --build build --parallel'
```

#### CRT shader (DXIL)

Windows D3D12 cannot load HLSL at runtime. After editing `assets/shaders/crt.frag.hlsl`, compile DXIL on a Windows host and commit `assets/shaders/crt.frag.dxil`. Casual builders do not need DXC.

1. Install [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler/releases) (`dxc.exe`, `dxcompiler.dll`, `dxil.dll`) into `tools/dxc/` (gitignored) or onto `PATH`. `dxil.dll` must sit next to `dxc.exe` so the output is signed.
2. From the repo root:

```
C:\msys64\usr\bin\env.exe MSYSTEM=MINGW64 CHERE_INVOKING=1 C:\msys64\usr\bin\bash.exe --login -lc 'bash scripts/compile_crt_shader.sh'
```

Or set `DXC` to a `dxc.exe` from the Windows SDK / Vulkan SDK. CMake only copies the committed `.dxil`; it does not run DXC.

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
