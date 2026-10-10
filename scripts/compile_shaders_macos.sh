#!/usr/bin/env bash
# Compile SDL3-convention HLSL fragment shaders to Metal source on macOS:
#   assets/shaders/<name>.frag.hlsl -> SPIR-V (glslang) -> <name>.frag.metal (spirv-cross)
# then compile-check the generated MSL with Xcode's metal compiler.
# Commit the resulting .metal. Casual builders do not need these tools.
#
# Requires: brew install glslang spirv-cross; Xcode (xcrun metal).
#
# Usage: scripts/compile_shaders_macos.sh [name...]
# Default: ntsc gscomposite. (crt.frag.metal is hand-maintained; it is not regenerated here.)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHADERS="$ROOT/assets/shaders"

if [[ $# -eq 0 ]]; then
    set -- ntsc gscomposite
fi

for tool in glslangValidator spirv-cross; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "compile_shaders_macos.sh: $tool not found (brew install glslang spirv-cross)." >&2
        exit 1
    fi
done
# The MSL compile-check needs Xcode's optional Metal Toolchain component
# (xcodebuild -downloadComponent MetalToolchain). Without it, skip the check;
# SDL still compiles the MSL source at runtime (vsgtest --gpu exercises it).
METAL_CHECK=1
if ! xcrun -sdk macosx metal -v >/dev/null 2>&1; then
    echo "compile_shaders_macos.sh: Metal Toolchain not installed; skipping xcrun metal check." >&2
    echo "  Install with: xcodebuild -downloadComponent MetalToolchain" >&2
    METAL_CHECK=0
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

for name in "$@"; do
    src="$SHADERS/$name.frag.hlsl"
    dst="$SHADERS/$name.frag.metal"
    if [[ ! -f "$src" ]]; then
        echo "compile_shaders_macos.sh: missing $src" >&2
        exit 1
    fi

    echo "Compiling $name: HLSL -> SPIR-V"
    glslangValidator -D -V -S frag -e main "$src" -o "$TMP/$name.spv"

    echo "Compiling $name: SPIR-V -> MSL"
    spirv-cross "$TMP/$name.spv" --msl --msl-version 20100 --output "$dst"

    if [[ "$METAL_CHECK" -eq 1 ]]; then
        echo "Checking $name: xcrun metal"
        xcrun -sdk macosx metal -c "$dst" -o "$TMP/$name.air"
    fi

    echo "Wrote $dst"
done
