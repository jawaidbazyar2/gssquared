#!/usr/bin/env bash
# Compile assets/shaders/crt.frag.hlsl to DXIL (Windows/D3D12) and/or SPIR-V
# (Linux/Vulkan). Run after editing the HLSL. Casual builders do not need DXC;
# commit the resulting crt.frag.dxil / crt.frag.spv.
#
# Usage: scripts/compile_crt_shader.sh [dxil|spirv|all]
# Default: all (skips a target if this DXC cannot emit it).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/assets/shaders/crt.frag.hlsl"
DST_DXIL="$ROOT/assets/shaders/crt.frag.dxil"
DST_SPV="$ROOT/assets/shaders/crt.frag.spv"

TARGET="${1:-all}"
case "$TARGET" in
    dxil|spirv|all) ;;
    *)
        echo "Usage: $0 [dxil|spirv|all]" >&2
        exit 1
        ;;
esac

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

if [[ ! -f "$SRC" ]]; then
    echo "compile_crt_shader.sh: missing $SRC" >&2
    exit 1
fi

# Prefer the DXC next to libdxcompiler.so (Linux tarball layout: bin/dxc + lib/).
DXC_DIR="$(cd "$(dirname "$DXC_BIN")" && pwd)"
if [[ -d "$DXC_DIR/../lib" ]]; then
    export LD_LIBRARY_PATH="$(cd "$DXC_DIR/../lib" && pwd)${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif [[ -d "$ROOT/tools/dxc/lib" ]]; then
    export LD_LIBRARY_PATH="$ROOT/tools/dxc/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

compile_dxil() {
    echo "Compiling CRT fragment shader to DXIL with $DXC_BIN"
    "$DXC_BIN" -T ps_6_0 -E main -Fo "$(to_native "$DST_DXIL")" "$(to_native "$SRC")" || return 1
    echo "Wrote $DST_DXIL"
}

compile_spirv() {
    echo "Compiling CRT fragment shader to SPIR-V with $DXC_BIN"
    # Flags match SDL_shadercross's DXC SPIR-V invocation so resource sets and
    # interpolants line up with SDL_GPU's Vulkan renderer.
    "$DXC_BIN" -T ps_6_0 -E main -spirv \
        -fspv-flatten-resource-arrays \
        -fspv-preserve-bindings \
        -fspv-preserve-interface \
        -Fo "$(to_native "$DST_SPV")" "$(to_native "$SRC")" || return 1
    echo "Wrote $DST_SPV"
}

compiled=0
if [[ "$TARGET" == "dxil" || "$TARGET" == "all" ]]; then
    if compile_dxil; then
        compiled=1
    elif [[ "$TARGET" == "dxil" ]]; then
        echo "compile_crt_shader.sh: DXIL compile failed." >&2
        exit 1
    else
        echo "compile_crt_shader.sh: DXIL compile failed; continuing (target=all)." >&2
    fi
fi

if [[ "$TARGET" == "spirv" || "$TARGET" == "all" ]]; then
    compile_spirv
    compiled=1
fi

if [[ "$compiled" -eq 0 ]]; then
    echo "compile_crt_shader.sh: nothing compiled." >&2
    exit 1
fi
