#!/usr/bin/env bash
# Compile assets/shaders/crt.frag.hlsl to DXIL for the Windows (D3D12) CRT shader.
# Run on a Windows host after editing the HLSL. Casual builders do not need DXC;
# commit the resulting assets/shaders/crt.frag.dxil.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/assets/shaders/crt.frag.hlsl"
DST="$ROOT/assets/shaders/crt.frag.dxil"

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
elif command -v dxc >/dev/null 2>&1; then
    DXC_BIN="$(command -v dxc)"
else
    echo "compile_crt_shader.sh: dxc not found." >&2
    echo "Install DirectX Shader Compiler (dxc.exe, dxcompiler.dll, dxil.dll) into tools/dxc/" >&2
    echo "from https://github.com/microsoft/DirectXShaderCompiler/releases, or set DXC." >&2
    exit 1
fi

if [[ ! -f "$SRC" ]]; then
    echo "compile_crt_shader.sh: missing $SRC" >&2
    exit 1
fi

echo "Compiling CRT fragment shader with $DXC_BIN"
"$DXC_BIN" -T ps_6_0 -E main -Fo "$(to_native "$DST")" "$(to_native "$SRC")"
echo "Wrote $DST"
