#!/usr/bin/env python3
"""Play a SenseiPlay track, press ESC, check for UNCLAIMED SOUND INTERRUPT.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 \\
    clients/python/examples/debug_senseiplay_esc.py \\
    [/path/to/senseiplay800.2mg]
"""

from __future__ import annotations

import json
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    DEVICE_ID_ENSONIQ,
    MEM_MAIN,
    MEM_MEGAII,
    PLATFORM_APPLE_IIGS,
    Client,
    ProtocolError,
    SCANCODE_DOWN,
    SCANCODE_RETURN,
)
from gs2debug.keys import SCANCODE_ESCAPE

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
DEFAULT_DISK = Path("/Users/bazyar/src/IIgsDisks/senseiplay800.2mg")
DEBUG_LOG = Path("/Users/bazyar/src/gssquared/.cursor/debug-7842e6.log")
ENSONIQ_STATE_V1_SIZE = 784
E1002C = 0xE1002C


def find_gs2() -> Path:
    env = os.environ.get("GS2_BIN")
    path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


def agent_log(hypothesis_id: str, location: str, message: str, data: dict) -> None:
    payload = {
        "sessionId": "7842e6",
        "runId": os.environ.get("SENSEI_RUN_ID", "esc-test"),
        "hypothesisId": hypothesis_id,
        "location": location,
        "message": message,
        "data": data,
        "timestamp": int(time.time() * 1000),
    }
    DEBUG_LOG.parent.mkdir(parents=True, exist_ok=True)
    with DEBUG_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(payload, separators=(",", ":")) + "\n")


def wait_for_socket(path: Path, timeout_s: float = 30.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if path.exists():
            try:
                with Client() as probe:
                    probe.connect(str(path))
                return
            except OSError:
                pass
        time.sleep(0.1)
    raise TimeoutError(f"debug socket not ready: {path}")


def tap(c: Client, scancode: int, *, hold_s: float = 0.05, gap_s: float = 0.3) -> None:
    c.tap_key(scancode, hold_s=hold_s)
    time.sleep(gap_s)


def decode_text(data: bytes) -> str:
    return "".join(chr(b & 0x7F) if 0x20 <= (b & 0x7F) <= 0x7E else "." for b in data)


def screen_text(c: Client) -> str:
    rows = []
    for row in range(0, 24):
        # 40-col text page 1 layout (Mega II)
        base = 0x0400 + ((row % 8) * 0x80) + ((row // 8) * 0x28)
        rows.append(decode_text(c.read_mem(MEM_MEGAII, base, 40)))
    return "\n".join(rows)


def parse_ensoniq(blob: bytes) -> dict:
    if len(blob) != ENSONIQ_STATE_V1_SIZE:
        raise RuntimeError(f"bad ensoniq blob len {len(blob)}")
    soundctl = blob[4]
    rege0 = blob[8]
    oscs = blob[10]
    o31 = blob[16 + 31 * 24 : 16 + 32 * 24]
    (freq,) = struct.unpack_from("<H", o31, 0)
    control = o31[4]
    irqpend = o31[14]
    (acc,) = struct.unpack_from("<I", o31, 16)
    pending = []
    for o in range(32):
        rec = blob[16 + o * 24 : 16 + (o + 1) * 24]
        if rec[14]:
            pending.append(o)
    return {
        "busy": bool(soundctl & 0x80),
        "soundctl": soundctl,
        "rege0": rege0,
        "oscs": oscs,
        "osc31": {
            "freq": freq,
            "ctrl": control,
            "irqpend": irqpend,
            "acc": acc,
            "halted": bool(control & 1),
            "ie": bool(control & 0x08),
        },
        "irqpend_oscs": pending,
    }


def snapshot(c: Client, tag: str) -> dict:
    blob = c.state_get(DEVICE_ID_ENSONIQ)
    enso = parse_ensoniq(blob)
    vec = c.read_mem(MEM_MAIN, E1002C, 4)
    text = screen_text(c)
    unclaimed = "UNCLAIMED" in text.upper() or "SOUND INTERRUPT" in text.upper()
    sample = {
        "tag": tag,
        "ensoniq": enso,
        "e1002c": vec.hex(),
        "unclaimed_on_screen": unclaimed,
        "text_snip": text.replace("\n", " | ")[:240],
    }
    agent_log("G", "debug_senseiplay_esc.py:snapshot", "snapshot", sample)
    print(
        f"[{tag}] unclaimed={unclaimed} e1002c={vec.hex()} "
        f"osc31={enso['osc31']} irqpend={enso['irqpend_oscs']} e0=${enso['rege0']:02X}",
        flush=True,
    )
    if unclaimed:
        print("--- screen ---", flush=True)
        print(text, flush=True)
    return sample


def main() -> int:
    disk = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DISK
    if not disk.is_file():
        raise FileNotFoundError(f"disk image not found: {disk}")

    sock = Path(f"/tmp/gs2-sensei-esc-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    gs2 = find_gs2()
    cmd = [
        str(gs2),
        "--debug",
        str(sock),
        "-p",
        "5",
        f"-ds7d1={disk}",
        "--no-quit-confirm",
    ]
    agent_log("G", "debug_senseiplay_esc.py:main", "launch", {"cmd": cmd})
    print("Launch:", " ".join(cmd), flush=True)
    proc = subprocess.Popen(
        cmd,
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
            assert st.platform_id == PLATFORM_APPLE_IIGS, st

            boot_s = float(os.environ.get("SENSEI_BOOT_S", "12"))
            print(f"Waiting {boot_s:.1f}s for boot/menu…", flush=True)
            time.sleep(boot_s)

            print("Navigate to song + play", flush=True)
            for _ in range(5):
                tap(c, SCANCODE_DOWN)
            tap(c, SCANCODE_RETURN, gap_s=0.7)
            for _ in range(2):
                tap(c, SCANCODE_DOWN)
            tap(c, SCANCODE_RETURN, gap_s=0.8)

            time.sleep(1.5)
            snap_play = snapshot(c, "playing")

            print("Press ESC (exit player → NTPstop)", flush=True)
            tap(c, SCANCODE_ESCAPE, hold_s=0.08, gap_s=0.5)
            # Give stop + possible unclaimed dialog time to appear
            time.sleep(1.0)
            snaps = [snapshot(c, f"after_esc_{i}") for i in range(6)]
            time.sleep(0.25)

            unclaimed = any(s["unclaimed_on_screen"] for s in snaps) or snap_play.get(
                "unclaimed_on_screen"
            )
            # Also check if osc31 still has irqpend / IE after stop
            last = snaps[-1]
            verdict = {
                "unclaimed": unclaimed,
                "playing_osc31": snap_play["ensoniq"]["osc31"],
                "after_osc31": last["ensoniq"]["osc31"],
                "after_irqpend": last["ensoniq"]["irqpend_oscs"],
                "after_e1002c": last["e1002c"],
                "e1002c_changed": snap_play["e1002c"] != last["e1002c"],
            }
            agent_log("G", "debug_senseiplay_esc.py:verdict", "verdict", verdict)
            print("VERDICT:", json.dumps(verdict, indent=2), flush=True)

            c.quit()
            print("PASS" if not unclaimed else "FAIL: unclaimed sound interrupt", flush=True)
            return 0 if not unclaimed else 1
    finally:
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)
        if sock.exists():
            sock.unlink()


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
        agent_log("G", "debug_senseiplay_esc.py:fail", "fail", {"error": str(exc)})
        raise SystemExit(1)
