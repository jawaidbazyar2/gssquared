#!/usr/bin/env bash
#
# Build GSSquared, launch it with the agent enabled, and capture the wire
# stream to an xxd-style file for later inspection. Quit GSSquared (Cmd-Q
# on macOS) when you're done; the script reaps everything and reports the
# capture path.
#
# Usage:
#   tools/agent_capture.sh [output_file]
#
# Defaults:
#   socket = /tmp/iigs-agent.sock
#   output = /tmp/iigs-agent-capture-<timestamp>.txt
#
# Requires `socat` on your PATH (`brew install socat` on macOS).

set -euo pipefail

cd "$(dirname "$0")/.."

# Captures land inside the project tree at tools/captures/ so the agent
# (whichever one is helping the user — Claude in Cowork mode, etc.) can
# read them without further file shuffling. `latest.txt` is overwritten
# each run; a timestamped copy is kept alongside for history.
CAPTURE_DIR="$(pwd)/tools/captures"
mkdir -p "$CAPTURE_DIR"

SOCKET="${GS2_AGENT_SOCKET:-/tmp/iigs-agent.sock}"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
OUT="${1:-$CAPTURE_DIR/latest.txt}"
ARCHIVE="$CAPTURE_DIR/capture-$TIMESTAMP.txt"

# Default boot config: Apple IIgs (platform 5) booting "System 6 and
# Free Games" from slot 7 drive 1. Override these env vars to skip /
# replace the auto-launch; e.g. GS2_PLATFORM= to skip the -p flag,
# GS2_S7D1= to skip the disk mount.
GS2_PLATFORM="${GS2_PLATFORM-5}"
GS2_S7D1="${GS2_S7D1-/Users/mdj/Downloads/System 6 and Free Games.hdv}"

if ! command -v socat >/dev/null 2>&1; then
    echo "[capture] socat not found — \`brew install socat\` and retry" >&2
    exit 1
fi

echo "[capture] building (Debug)…"
cmake --build build --parallel

# Clean any stale socket from a previous crash.
rm -f "$SOCKET"

# If GS2_NO_CAPTURE is set, leave the socket free for an interactive
# client. Otherwise spawn the usual socat watcher that records the
# wire stream to $OUT.
SOCAT_PID=""
if [ -z "${GS2_NO_CAPTURE:-}" ]; then
    echo "[capture] starting socat (will wait for socket); recording to $OUT"
    (
        for _ in $(seq 1 40); do
            if [ -S "$SOCKET" ]; then break; fi
            sleep 0.25
        done
        if [ ! -S "$SOCKET" ]; then
            echo "[capture] timed out waiting for $SOCKET" >&2
            exit 1
        fi
        exec socat -u UNIX-CONNECT:"$SOCKET" - | xxd > "$OUT"
    ) &
    SOCAT_PID=$!
else
    echo "[capture] GS2_NO_CAPTURE set — leaving socket free for interactive clients"
fi

cat <<'EOF'
[capture] interact with the IIgs in the GSSquared window now.
[capture] quit the emulator (Cmd-Q on macOS) when you're done capturing.
EOF

echo "[capture] starting GSSquared with agent on $SOCKET"
# Build the GSSquared argument list. Quoting handles paths with spaces
# (e.g. "System 6 and Free Games.hdv"). `-d` accepts the form
# `s<slot>d<drive>=<file>` per the regex in gs2.cpp.
gs2_args=()
if [ -n "${GS2_PLATFORM}" ]; then
    gs2_args+=( -p "${GS2_PLATFORM}" )
    echo "[capture]   platform=${GS2_PLATFORM} (5 = IIgs)"
fi
if [ -n "${GS2_S7D1}" ]; then
    gs2_args+=( "-ds7d1=${GS2_S7D1}" )
    echo "[capture]   s7d1=${GS2_S7D1}"
fi

# Log GSSquared's stderr to a fixed path so cohabiting tools (the
# agent's helper, Claude in Cowork, etc.) can read it without anyone
# copy-pasting from a terminal. Use `tee` so the user still sees output
# live. Truncated each run.
STDERR_LOG="$CAPTURE_DIR/stderr.log"
: > "$STDERR_LOG"
echo "[capture]   stderr log: $STDERR_LOG"

GS2_AGENT_SOCKET="$SOCKET" build/GSSquared "${gs2_args[@]}" 2> >(tee "$STDERR_LOG" >&2) || true

if [ -n "$SOCAT_PID" ]; then
    kill "$SOCAT_PID" 2>/dev/null || true
    wait "$SOCAT_PID" 2>/dev/null || true

    # Preserve a timestamped copy alongside `latest.txt`.
    if [ -f "$OUT" ]; then
        cp "$OUT" "$ARCHIVE"
    fi

    BYTES=$(wc -c < "$OUT" 2>/dev/null | tr -d ' ' || echo 0)
    LINES=$(wc -l < "$OUT" 2>/dev/null | tr -d ' ' || echo 0)
    echo "[capture] done. saved $BYTES bytes / $LINES lines"
    echo "[capture]   latest:  $OUT"
    echo "[capture]   archive: $ARCHIVE"
    echo "[capture] decode with: tools/decode_agent_stream.py $OUT | less"
else
    echo "[capture] done (no recording; GS2_NO_CAPTURE was set)"
fi
