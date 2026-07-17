#!/usr/bin/env python3
"""Boot SenseiPlay on IIgs, start a song, and dump Ensoniq state around the freeze.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 \\
    clients/python/examples/debug_senseiplay_ensoniq.py \\
    [/path/to/senseiplay800.2mg]

Launches ``./build/GSSquared --debug <sock> -p 5 -ds7d1=<disk> --no-quit-confirm``,
navigates the SenseiPlay menu (Down×5, Return, Down×2, Return), samples PC +
Ensoniq STATE_GET while playing, writes NDJSON to the debug log, then QUITs.
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
    MEM_MEGAII,
    PLATFORM_APPLE_IIGS,
    Client,
    ProtocolError,
    SCANCODE_DOWN,
    SCANCODE_RETURN,
)
REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
DEFAULT_DISK = Path("/Users/bazyar/src/IIgsDisks/senseiplay800.2mg")
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
        "runId": os.environ.get("SENSEI_RUN_ID", "post-fix"),
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


def drain_events(c: Client, seconds: float = 0.15) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            c.wait_event(timeout=max(0.01, deadline - time.monotonic()))
        except TimeoutError:
            break
        except (ProtocolError, OSError, ConnectionError):
            break


def tap(c: Client, scancode: int, *, hold_s: float = 0.05, gap_s: float = 0.25) -> None:
    c.tap_key(scancode, hold_s=hold_s)
    time.sleep(gap_s)


def decode_text40_row(data: bytes) -> str:
    out = []
    for b in data:
        ch = b & 0x7F
        out.append(chr(ch) if 0x20 <= ch <= 0x7E else ".")
    return "".join(out)


def peek_menu_text(c: Client) -> str:
    # Mega II text page 1, a few rows (SenseiPlay is 40-col text UI).
    rows = []
    for row in range(0, 8):
        addr = 0x0400 + row * 0x80
        data = c.read_mem(MEM_MEGAII, addr, 40)
        rows.append(decode_text40_row(data))
    return "\n".join(rows)


def parse_ensoniq(blob: bytes) -> dict:
    if len(blob) != ENSONIQ_STATE_V1_SIZE:
        raise RuntimeError(f"bad ensoniq blob len {len(blob)}")
    (version,) = struct.unpack_from("<I", blob, 0)
    soundctl = blob[4]
    rege0 = blob[8]
    rege1 = blob[9]
    oscs = blob[10]
    (rate,) = struct.unpack_from("<I", blob, 12)
    oscs_out = []
    for o in range(32):
        rec = blob[16 + o * 24 : 16 + (o + 1) * 24]
        (freq,) = struct.unpack_from("<H", rec, 0)
        (wtsize,) = struct.unpack_from("<H", rec, 2)
        control = rec[4]
        vol = rec[5]
        irqpend = rec[14]
        (acc,) = struct.unpack_from("<I", rec, 16)
        if o == 31 or control != 1 or irqpend or freq:
            oscs_out.append(
                {
                    "o": o,
                    "freq": freq,
                    "wtsize": wtsize,
                    "ctrl": control,
                    "vol": vol,
                    "irqpend": irqpend,
                    "acc": acc,
                    "halted": bool(control & 1),
                    "ie": bool(control & 0x08),
                    "mode": (control >> 1) & 3,
                }
            )
    return {
        "version": version,
        "soundctl": soundctl,
        "busy": bool(soundctl & 0x80),
        "rege0": rege0,
        "rege1": rege1,
        "oscsenabled": oscs,
        "rate": rate,
        "osc_interest": oscs_out,
        "osc31": next((x for x in oscs_out if x["o"] == 31), None),
    }


def latest_pc(c: Client) -> int | None:
    """PC from newest trace entry — does not pause (avoid killing Ensoniq catch-up)."""
    tw = c.get_trace(ago=0, count=1)
    if not tw.entries:
        return None
    entry = tw.entries[-1]
    if len(entry) < 18:
        return None
    pb = entry[15]
    (pc16,) = struct.unpack_from("<H", entry, 16)
    return (pb << 16) | pc16


def sample_once(c: Client, tag: str) -> dict:
    # Sample while RUNNING. Pause/continue resets ensoniq_catch_up and can fake a freeze.
    blob = c.state_get(DEVICE_ID_ENSONIQ)
    info = parse_ensoniq(blob)
    pc = latest_pc(c)
    sample = {
        "tag": tag,
        "pc": pc,
        "busy": info["busy"],
        "soundctl": info["soundctl"],
        "rege0": info["rege0"],
        "oscs": info["oscsenabled"],
        "rate": info["rate"],
        "osc31": info["osc31"],
        "active_oscs": [
            x
            for x in info["osc_interest"]
            if (not x["halted"]) or x["irqpend"] or x["o"] == 31
        ],
    }
    agent_log("C", "debug_senseiplay_ensoniq.py:sample", "sample", sample)
    pc_s = f"${pc:06X}" if pc is not None else "none"
    print(
        f"[{tag}] pc={pc_s} busy={info['busy']} ctl=${info['soundctl']:02X} "
        f"e0=${info['rege0']:02X} oscs={info['oscsenabled']} rate={info['rate']} "
        f"osc31={info['osc31']}",
        flush=True,
    )
    return sample


def main() -> int:
    disk = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DISK
    if not disk.is_file():
        raise FileNotFoundError(f"disk image not found: {disk}")

    sock = Path(f"/tmp/gs2-sensei-{os.getpid()}.sock")
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
    agent_log("C", "debug_senseiplay_ensoniq.py:main", "launch", {"cmd": cmd})
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

            # Boot + SenseiPlay load (ProDOS / UI).
            boot_s = float(os.environ.get("SENSEI_BOOT_S", "12"))
            print(f"Waiting {boot_s:.1f}s for boot/menu…", flush=True)
            time.sleep(boot_s)
            drain_events(c)

            try:
                menu = peek_menu_text(c)
                agent_log(
                    "C",
                    "debug_senseiplay_ensoniq.py:menu",
                    "menu_text",
                    {"text": menu},
                )
                print("--- menu text ---", flush=True)
                print(menu, flush=True)
                print("-----------------", flush=True)
            except ProtocolError as exc:
                print(f"menu peek failed: {exc}", flush=True)

            sample_once(c, "pre_nav")

            print("Navigate: Down×5, Return, Down×2, Return", flush=True)
            for _ in range(5):
                tap(c, SCANCODE_DOWN)
            tap(c, SCANCODE_RETURN, gap_s=0.6)
            for _ in range(2):
                tap(c, SCANCODE_DOWN)
            tap(c, SCANCODE_RETURN, gap_s=0.8)
            agent_log("C", "debug_senseiplay_ensoniq.py:nav", "nav_done", {})

            # Let the first audio burst happen, then sample for freeze (~5s).
            time.sleep(0.5)
            samples = []
            pcs: list[int] = []
            n_samples = int(os.environ.get("SENSEI_SAMPLES", "24"))
            for i in range(n_samples):
                s = sample_once(c, f"play_{i}")
                samples.append(s)
                pcs.append(s["pc"])
                time.sleep(0.2)

            # Freeze heuristic: last 6 PCs identical, or busy stuck, or osc31 halted+no irq.
            pcs_ok = [p for p in pcs if p is not None]
            # Trace PC advances every insn; freeze shows as a tiny unique-PC set over time.
            stuck = len(pcs_ok) >= 6 and len(set(pcs_ok[-6:])) <= 2
            busy_stuck = any(s["busy"] for s in samples[-4:])
            o31 = samples[-1]["osc31"] if samples else None
            o31_dead = bool(
                o31
                and o31.get("halted")
                and not o31.get("irqpend")
                and o31.get("freq", 0) > 0
            )
            # Osc31 free-run timer: accumulator should keep moving if catch-up runs.
            o31_accs = [
                s["osc31"]["acc"]
                for s in samples
                if s.get("osc31") and "acc" in s["osc31"]
            ]
            o31_acc_frozen = len(o31_accs) >= 4 and len(set(o31_accs[-4:])) == 1
            verdict = {
                "pc_stuck": stuck,
                "stuck_pc": pcs_ok[-1] if stuck and pcs_ok else None,
                "busy_seen_late": busy_stuck,
                "osc31_dead": o31_dead,
                "osc31_acc_frozen": o31_acc_frozen,
                "last": samples[-1] if samples else None,
                "unique_pcs": sorted({f"{p:06X}" for p in pcs_ok}),
            }
            agent_log("C", "debug_senseiplay_ensoniq.py:verdict", "verdict", verdict)
            print("VERDICT:", json.dumps(verdict, indent=2), flush=True)

            # Final deep sample while paused.
            c.pause()
            final = c.wait_stopped(timeout=2.0)
            blob = c.state_get(DEVICE_ID_ENSONIQ)
            info = parse_ensoniq(blob)
            agent_log(
                "C",
                "debug_senseiplay_ensoniq.py:final",
                "final_state",
                {"pc": final.pc, "ensoniq": info},
            )
            print(
                f"FINAL pc=${final.pc:06X} busy={info['busy']} osc31={info['osc31']}",
                flush=True,
            )

            c.quit()
            print("done (QUIT)", flush=True)
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
        agent_log("C", "debug_senseiplay_ensoniq.py:fail", "fail", {"error": str(exc)})
        raise SystemExit(1)
