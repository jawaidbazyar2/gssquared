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
