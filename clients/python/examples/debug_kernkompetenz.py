#!/usr/bin/env python3
"""Boot WITA2GS, launch Kernkompetenz, sample Ensoniq state during playback.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 \\
    clients/python/examples/debug_kernkompetenz.py \\
    [/path/to/wita2gs.pmap]
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
    PLATFORM_APPLE_IIGS,
    Client,
    ProtocolError,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
DEFAULT_PMAP = Path("/Users/bazyar/src/IIgsDisks/wita2gs_0_81/wita2gs.pmap")
DEBUG_LOG = Path("/Users/bazyar/src/gssquared/.cursor/debug-7842e6.log")
ENSONIQ_STATE_V1_SIZE = 784


def find_gs2() -> Path:
    env = os.environ.get("GS2_BIN")
    path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


def agent_log(hypothesis_id: str, location: str, message: str, data: dict) -> None:
    payload = {
        "sessionId": "7842e6",
        "runId": os.environ.get("KERN_RUN_ID", "kern-pre"),
        "hypothesisId": hypothesis_id,
        "location": location,
        "message": message,
        "data": data,
        "timestamp": int(time.time() * 1000),
    }
    DEBUG_LOG.parent.mkdir(parents=True, exist_ok=True)
    with DEBUG_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(payload, separators=(",", ":")) + "\n")


def wait_for_socket(path: Path, timeout_s: float = 45.0) -> None:
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


def parse_ensoniq(blob: bytes) -> dict:
    if len(blob) != ENSONIQ_STATE_V1_SIZE:
        raise RuntimeError(f"bad ensoniq blob len {len(blob)}")
    oscs = blob[10]
    (rate,) = struct.unpack_from("<I", blob, 12)
    active = []
    for o in range(32):
        rec = blob[16 + o * 24 : 16 + (o + 1) * 24]
        (freq,) = struct.unpack_from("<H", rec, 0)
        (wtsize,) = struct.unpack_from("<H", rec, 2)
        control = rec[4]
        vol = rec[5]
        (acc,) = struct.unpack_from("<I", rec, 16)
        if not (control & 1) or vol or freq:
            active.append(
                {
                    "o": o,
                    "freq": freq,
                    "wtsize": wtsize,
                    "ctrl": control,
                    "vol": vol,
                    "acc": acc,
                    "mode": (control >> 1) & 3,
                    "halted": bool(control & 1),
                }
            )
    return {"oscs": oscs, "rate": rate, "active": active}


def main() -> int:
    pmap = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_PMAP
    if not pmap.is_file():
        raise FileNotFoundError(f"pmap not found: {pmap}")

    sock = Path(f"/tmp/gs2-kern-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    gs2 = find_gs2()
    cmd = [
        str(gs2),
        "--debug",
        str(sock),
        "-p",
        "5",
        f"-ds7d1={pmap}",
        "--no-quit-confirm",
    ]
    agent_log("N", "debug_kernkompetenz.py:main", "launch", {"cmd": cmd})
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

            boot_s = float(os.environ.get("KERN_BOOT_S", "5"))
            print(f"Waiting {boot_s:.1f}s after boot…", flush=True)
            time.sleep(boot_s)

            print("Typing: kern + Enter", flush=True)
            c.type_text("kern\n", delay_s=0.08, hold_s=0.04)
            agent_log("N", "debug_kernkompetenz.py:typed", "typed_kern", {})

            # Wait for demo load, then capture ~12s after music starts (artifact window).
            load_s = float(os.environ.get("KERN_LOAD_S", "45"))
            audio_s = float(os.environ.get("KERN_AUDIO_S", "14"))
            print(
                f"Waiting up to {load_s:.1f}s for music, then {audio_s:.1f}s of audio…",
                flush=True,
            )
            samples = []
            load_deadline = time.monotonic() + load_s
            i = 0
            music_seen_at = None
            while True:
                now = time.monotonic()
                if music_seen_at is None and now >= load_deadline:
                    break
                if music_seen_at is not None and (now - music_seen_at) >= audio_s:
                    break
                blob = c.state_get(DEVICE_ID_ENSONIQ)
                info = parse_ensoniq(blob)
                free_run = [a for a in info["active"] if a["mode"] == 0 and not a["halted"]]
                voiced = [
                    a for a in info["active"] if a["vol"] and not a["halted"]
                ]
                sample = {
                    "i": i,
                    "oscs": info["oscs"],
                    "rate": info["rate"],
                    "n_active": len(info["active"]),
                    "n_voiced": len(voiced),
                    "n_free": len(free_run),
                    "free": free_run[:4],
                    "active": voiced[:8] or info["active"][:8],
                }
                samples.append(sample)
                agent_log("N", "debug_kernkompetenz.py:sample", "sample", sample)
                if music_seen_at is None and len(voiced) >= 2:
                    music_seen_at = time.monotonic()
                    agent_log(
                        "AH",
                        "debug_kernkompetenz.py:music",
                        "music_start",
                        {"i": i, "n_voiced": len(voiced), "rate": info["rate"]},
                    )
                    print(f"[{i}] music start rate={info['rate']} voiced={len(voiced)}", flush=True)
                if i % 10 == 0:
                    print(
                        f"[{i}] rate={info['rate']} oscs={info['oscs']} "
                        f"voiced={len(voiced)} free={len(free_run)}",
                        flush=True,
                    )
                i += 1
                time.sleep(0.2)

            verdict = {
                "samples": len(samples),
                "rates": sorted({s["rate"] for s in samples}),
                "max_active": max(s["n_active"] for s in samples) if samples else 0,
                "max_voiced": max(s.get("n_voiced", 0) for s in samples) if samples else 0,
                "max_free": max(s["n_free"] for s in samples) if samples else 0,
                "music_seen": music_seen_at is not None,
            }
            agent_log("N", "debug_kernkompetenz.py:verdict", "verdict", verdict)
            print("VERDICT:", json.dumps(verdict, indent=2), flush=True)

            c.quit()
            print("done", flush=True)
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
        agent_log("N", "debug_kernkompetenz.py:fail", "fail", {"error": str(exc)})
        raise SystemExit(1)
