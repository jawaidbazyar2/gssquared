#!/usr/bin/env bash
#
# MCP launcher for GSSquared. Registered by .mcp.json: ensures a headless
# emulator is running, then bridges this process's stdio to its MCP socket
# so an MCP client (e.g. Claude Code) can drive it. Newline-delimited
# JSON-RPC passes through socat unchanged.
#
# Env overrides:
#   GS2_MCP_SOCKET  socket path         (default /tmp/gs2-mcp.sock)
#   GS2_BIN         GSSquared binary    (default <repo>/build/GSSquared)
#   GS2_PLATFORM    -p platform id      (default 1 = Apple II+)
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"   # repo root
SOCK="${GS2_MCP_SOCKET:-/tmp/gs2-mcp.sock}"
BIN="${GS2_BIN:-$HERE/build/GSSquared}"
PLATFORM="${GS2_PLATFORM:-1}"

if ! command -v socat >/dev/null 2>&1; then
    echo "mcp_launch: socat not found (brew install socat)" >&2
    exit 1
fi

# Start a headless emulator if one isn't already listening on the socket.
if [ ! -S "$SOCK" ]; then
    GS2_HEADLESS=1 GS2_MCP_SOCKET="$SOCK" SDL_AUDIODRIVER=dummy \
        "$BIN" -p "$PLATFORM" >/tmp/gs2-mcp-emu.log 2>&1 &
    for _ in $(seq 1 80); do
        [ -S "$SOCK" ] && break
        sleep 0.25
    done
    if [ ! -S "$SOCK" ]; then
        echo "mcp_launch: emulator did not create $SOCK (see /tmp/gs2-mcp-emu.log)" >&2
        exit 1
    fi
fi

exec socat - "UNIX-CONNECT:$SOCK"
