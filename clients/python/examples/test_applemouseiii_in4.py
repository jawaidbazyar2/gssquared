#!/usr/bin/env python3
"""IIe Enhanced smoke: AppleMouse III via Applesoft IN#4.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/test_applemouseiii_in4.py

Launches ``./build/GSSquared --debug <sock> --no-quit-confirm assets/gs2/IIe_AppleMouseIII.gs2``,
waits, Control-Resets into Applesoft, types a short IN#4 program, injects motion via
STATE_SET, and asserts the printed coordinates.
"""

from __future__ import annotations

import os
import re
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    DEVICE_ID_MOUSE,
    KMOD_CTRL,
    KMOD_LCTRL,
    MEM_MAIN,
    PLATFORM_APPLE_IIE_ENHANCED,
    SCANCODE_F12,
    SCANCODE_LCTRL,
    Client,
    ProtocolError,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
GS2_CONFIG = REPO_ROOT / "assets" / "gs2" / "IIe_AppleMouseIII.gs2"
APPLEMOUSEIII_STATE_GET_V1_SIZE = 32
SLOT = 4
PROMPT_APPLESOFT = 0xDD  # ']'
PROMPT_MONITOR = 0xAA  # '*'

# ROM BASIC (no DOS): wake mouse, then loop IN#4 reads and print coords.
BASIC_PROGRAM = """\
NEW
10 HOME
20 PR#4
30 PRINT CHR$(1)
40 PR#0
50 IN#4
60 INPUT "";X,Y,S
70 IN#0
80 VTAB 10: HTAB 1
90 PRINT "X=";X;" Y=";Y;" S=";S;"   "
100 GOTO 50
"""


def find_gs2() -> Path:
    env = os.environ.get("GS2_BIN")
    path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


def wait_for_socket(path: Path, timeout_s: float = 45.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if path.exists():
            try:
                with Client() as probe:
                    probe.connect(str(path))
                    probe.hello()
                return
            except (OSError, ProtocolError, RuntimeError):
                pass
        time.sleep(0.2)
    raise TimeoutError(f"debug socket not ready: {path}")


def prompt_char(c: Client) -> int:
    return c.read_mem(MEM_MAIN, 0x33, 1)[0]


def control_reset(c: Client) -> None:
    """Keyboard Control+Reset (F12 with Ctrl mod). See Docs/gs2debug.md."""
    c.key_down(SCANCODE_LCTRL, KMOD_LCTRL)
    time.sleep(0.05)
    c.key_down(SCANCODE_F12, KMOD_CTRL)
    time.sleep(0.1)
    c.key_up(SCANCODE_F12, KMOD_CTRL)
    time.sleep(0.05)
    c.key_up(SCANCODE_LCTRL, 0)


def wait_for_applesoft(c: Client, timeout_s: float = 20.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if prompt_char(c) == PROMPT_APPLESOFT:
            return
        time.sleep(0.25)
    raise TimeoutError(f"Applesoft prompt not seen (prompt=${prompt_char(c):02X})")


def assert_still_applesoft(c: Client, where: str) -> None:
    p = prompt_char(c)
    if p == PROMPT_MONITOR:
        raise RuntimeError(f"{where}: fell into monitor (prompt=$AA)")
    if p != PROMPT_APPLESOFT:
        raise RuntimeError(f"{where}: expected Applesoft prompt $DD, got ${p:02X}")


def decode_text40_row(row: bytes) -> str:
    return "".join(
        chr(b & 0x7F) if 0x20 <= (b & 0x7F) <= 0x7E else " " for b in row
    )


# Apple II 40-column text page 1 line bases (VTAB 1..24).
_TEXT_LINE_BASES = (
    0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x780,
    0x428, 0x4A8, 0x528, 0x5A8, 0x628, 0x6A8, 0x728, 0x7A8,
    0x450, 0x4D0, 0x550, 0x5D0, 0x650, 0x6D0, 0x750, 0x7D0,
)


def read_screen_text(c: Client) -> str:
    return "\n".join(
        decode_text40_row(c.read_mem(MEM_MAIN, base, 0x28)) for base in _TEXT_LINE_BASES
    )


def pack_state_set(*, dx: int = 0, dy: int = 0, buttons: int = 0, flags: int = 0x01) -> bytes:
    return struct.pack("<IBbbB", 1, flags & 0xFF, int(dx), int(dy), buttons & 0xFF)


def parse_state_get(blob: bytes) -> dict:
    assert len(blob) == APPLEMOUSEIII_STATE_GET_V1_SIZE, len(blob)
    version = struct.unpack_from("<I", blob, 0)[0]
    slot, bank, mode, int_state = blob[4], blob[5], blob[6], blob[7]
    x, y = struct.unpack_from("<hh", blob, 12)
    min_x, min_y, max_x, max_y = struct.unpack_from("<hhhh", blob, 16)
    return {
        "version": version,
        "slot": slot,
        "bank": bank,
        "mode": mode,
        "int_state": int_state,
        "x": x,
        "y": y,
        "min_x": min_x,
        "min_y": min_y,
        "max_x": max_x,
        "max_y": max_y,
    }


def find_xy_print(screen: str) -> tuple[int, int, int] | None:
    """Parse ``X=<n> Y=<n> S=<n>`` from screen text."""
    m = re.search(r"X=\s*(-?\d+)\s+Y=\s*(-?\d+)\s+S=\s*(-?\d+)", screen)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


def wait_for_xy(
    c: Client,
    *,
    expect_x: int | None = None,
    expect_y: int | None = None,
    timeout_s: float = 10.0,
) -> tuple[int, int, int]:
    """Poll text page until an X=/Y=/S= line appears (optionally matching coords)."""
    deadline = time.monotonic() + timeout_s
    last = ""
    while time.monotonic() < deadline:
        screen = read_screen_text(c)
        last = screen
        parsed = find_xy_print(screen)
        if parsed is not None:
            x, y, s = parsed
            if expect_x is None and expect_y is None:
                return x, y, s
            if (expect_x is None or x == expect_x) and (expect_y is None or y == expect_y):
                return x, y, s
        time.sleep(0.2)
    raise TimeoutError(
        f"timed out waiting for X=/Y=/S= "
        f"(expect x={expect_x} y={expect_y}); screen=\n{last}"
    )


def main() -> int:
    if not GS2_CONFIG.is_file():
        raise FileNotFoundError(f"missing config: {GS2_CONFIG}")

    sock = Path(f"/tmp/gs2-applemouseiii-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    gs2 = find_gs2()
    proc = subprocess.Popen(
        [
            str(gs2),
            "--debug",
            str(sock),
            "--no-quit-confirm",
            str(GS2_CONFIG),
        ],
        cwd=str(REPO_ROOT),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    try:
        wait_for_socket(sock)
        with Client() as c:
            c.connect(str(sock))
            c.hello()
            st = c.get_status()
            assert st.platform_id == PLATFORM_APPLE_IIE_ENHANCED, st

            # Disk II may be scanning; wait then Control-Reset into Applesoft.
            time.sleep(4.0)
            control_reset(c)
            time.sleep(2.0)
            wait_for_applesoft(c)
            assert_still_applesoft(c, "after Control-Reset")

            blob = c.state_get(DEVICE_ID_MOUSE)
            state = parse_state_get(blob)
            assert state["version"] == 1, state
            assert state["slot"] == SLOT, state
            assert state["max_x"] == 1023 and state["max_y"] == 1023, state
            print(
                f"STATE_GET ok x={state['x']} y={state['y']} bank={state['bank']}",
                flush=True,
            )

            # Leave 80-col output hook if present, then enter the program.
            c.type_text("PR#0\n", delay_s=0.1)
            time.sleep(0.6)
            assert_still_applesoft(c, "after PR#0")

            c.type_text(BASIC_PROGRAM, delay_s=0.08)
            time.sleep(0.5)
            assert_still_applesoft(c, "after typing program")

            # Continuous loop (GOTO 50); leave it running and poll the screen.
            c.type_text("RUN\n", delay_s=0.1)
            x1, y1, s1 = wait_for_xy(c, timeout_s=12.0)
            print(f"IN#4 first read X={x1} Y={y1} S={s1}", flush=True)
            print(f"screen:\n{read_screen_text(c)}", flush=True)

            c.state_set(DEVICE_ID_MOUSE, pack_state_set(dx=80, dy=40, flags=0x01))
            state2 = parse_state_get(c.state_get(DEVICE_ID_MOUSE))
            assert state2["x"] != 0 or state2["y"] != 0, state2
            print(f"STATE_SET inject ok x={state2['x']} y={state2['y']}", flush=True)

            x2, y2, s2 = wait_for_xy(
                c, expect_x=state2["x"], expect_y=state2["y"], timeout_s=12.0
            )
            print(f"IN#4 loop read X={x2} Y={y2} S={s2}", flush=True)
            print(f"screen:\n{read_screen_text(c)}", flush=True)

            c.quit()
            print("PASS", flush=True)
    finally:
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)
        if sock.exists():
            sock.unlink()
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (
        OSError,
        ProtocolError,
        RuntimeError,
        TimeoutError,
        AssertionError,
        FileNotFoundError,
    ) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
