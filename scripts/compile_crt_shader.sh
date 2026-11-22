#!/usr/bin/env bash
# Compile SDL3-convention HLSL fragment shaders to DXIL (Windows/D3D12) and/or
# SPIR-V (Linux/Vulkan). Run after editing the HLSL. Casual builders do not
# need DXC; commit the resulting <name>.frag.dxil / <name>.frag.spv.
#
# These HLSL sources already use the semantics SDL_shadercross would rewrite
# (TEXCOORD0 / TEXCOORD1). Compile them with DXC directly. The shadercross CLI
# would shift those semantics again.
#
# Usage: scripts/compile_crt_shader.sh [dxil|spirv|all] [name...]
# Default: all crt
#   scripts/compile_crt_shader.sh all ntsc gscomposite
# Default target skips a format this DXC cannot emit.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

TARGET="all"
if [[ $# -gt 0 ]]; then
    case "$1" in
        dxil|spirv|all) TARGET="$1"; shift ;;
    esac
fi
if [[ $# -eq 0 ]]; then
    set -- crt
fi

to_native() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        printf '%s\n' "$1"
    fi
}

if [[ -n "${DXC:-}" ]]; then
    DXC_BIN="$DXC"
elif [[ -x "$ROOT/tools/dxc/dxc.exe" ]]; then
    DXC_BIN="$ROOT/tools/dxc/dxc.exe"
elif [[ -x "$ROOT/tools/dxc/dxc" ]]; then
    DXC_BIN="$ROOT/tools/dxc/dxc"
elif [[ -x "$ROOT/tools/dxc/bin/dxc" ]]; then
    DXC_BIN="$ROOT/tools/dxc/bin/dxc"
elif command -v dxc >/dev/null 2>&1; then
    DXC_BIN="$(command -v dxc)"
else
    echo "compile_crt_shader.sh: dxc not found." >&2
    echo "Install DirectX Shader Compiler into tools/dxc/ from" >&2
    echo "https://github.com/microsoft/DirectXShaderCompiler/releases, or set DXC." >&2
    echo "Linux: unpack linux_dxc_*.x86_64.tar.gz into tools/dxc/ (bin/dxc + lib/)." >&2
    exit 1
fi

# Prefer the DXC next to libdxcompiler.so (Linux tarball layout: bin/dxc + lib/).
DXC_DIR="$(cd "$(dirname "$DXC_BIN")" && pwd)"
if [[ -d "$DXC_DIR/../lib" ]]; then
    export LD_LIBRARY_PATH="$(cd "$DXC_DIR/../lib" && pwd)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif [[ -d "$ROOT/tools/dxc/lib" ]]; then
    export LD_LIBRARY_PATH="$ROOT/tools/dxc/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

compile_one() {
    local name="$1"
    local src="$ROOT/assets/shaders/$name.frag.hlsl"
    local dst_dxil="$ROOT/assets/shaders/$name.frag.dxil"
    local dst_spv="$ROOT/assets/shaders/$name.frag.spv"
    if [[ ! -f "$src" ]]; then
        echo "compile_crt_shader.sh: missing $src" >&2
        exit 1
    fi

    local compiled=0
    if [[ "$TARGET" == "dxil" || "$TARGET" == "all" ]]; then
        echo "Compiling $name fragment shader to DXIL with $DXC_BIN"
        if "$DXC_BIN" -T ps_6_0 -E main -Fo "$(to_native "$dst_dxil")" "$(to_native "$src")"; then
            echo "Wrote $dst_dxil"
            compiled=1
        elif [[ "$TARGET" == "dxil" ]]; then
            echo "compile_crt_shader.sh: DXIL compile failed for $name." >&2
            exit 1
        else
            echo "compile_crt_shader.sh: DXIL compile failed for $name; continuing (target=all)." >&2
        fi
    fi

    if [[ "$TARGET" == "spirv" || "$TARGET" == "all" ]]; then
        echo "Compiling $name fragment shader to SPIR-V with $DXC_BIN"
        # Flags match SDL_shadercross's DXC SPIR-V invocation so resource sets and
        # interpolants line up with SDL_GPU's Vulkan renderer.
        "$DXC_BIN" -T ps_6_0 -E main -spirv \
            -fspv-flatten-resource-arrays \
            -fspv-preserve-bindings \
            -fspv-preserve-interface \
            -Fo "$(to_native "$dst_spv")" "$(to_native "$src")"
        echo "Wrote $dst_spv"
        compiled=1
    fi

    if [[ "$compiled" -eq 0 ]]; then
        echo "compile_crt_shader.sh: nothing compiled for $name." >&2
        exit 1
    fi
}

for name in "$@"; do
    compile_one "$name"
done
