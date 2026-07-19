#!/usr/bin/env python3
"""Sample Disk II STATE_GET while booting A2OSX; log track/phase thrash.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/debug_a2osx_diskii.py

Or attach to an already-running emu:

  PYTHONPATH=clients/python/src python3 clients/python/examples/debug_a2osx_diskii.py \\
      --sock /tmp/gs2.sock --no-launch

Launches IIe Enhanced (-p 3) with A2OSX on slot 6 drive 1 by default.
Polls DEVICE_ID_DISK_II state and prints a line whenever drive-0 track or
phases change. Writes NDJSON to /tmp/gs2-a2osx-diskii.ndjson.
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    DEVICE_ID_DISK_II,
    PLATFORM_APPLE_IIE_ENHANCED,
    Client,
    ProtocolError,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
DEFAULT_DISK = Path("/Users/bazyar/src/AppleIIDisks/A2OSX.STABLE.140.po")
DISKII_STATE_V1_SIZE = 60


def find_gs2() -> Path:
    env = os.environ.get("GS2_BIN")
    path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


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


def parse_diskii_state(blob: bytes) -> dict:
    if len(blob) != DISKII_STATE_V1_SIZE:
        raise ValueError(f"unexpected Disk II state size {len(blob)}")
    version, = struct.unpack_from("<I", blob, 0)
    select = blob[4]
    motor_on = blob[5]
    motor_latch = blob[6]
    q6, q7 = blob[7], blob[8]
    data_reg = blob[9]
    seq = blob[10]
    mark_off, cycles = struct.unpack_from("<QQ", blob, 12)

    drives = []
    for d in range(2):
        base = 28 + d * 16
        track, max_tracks = struct.unpack_from("<hh", blob, base)
        phases = list(blob[base + 4 : base + 8])
        enable = blob[base + 8]
        wp = blob[base + 9]
        mounted = blob[base + 10]
        drives.append(
            {
                "track_q": track,
                "track": f"{track // 4}.{track % 4}",
                "max_tracks": max_tracks,
                "phases": phases,
                "enable": enable,
                "wp": wp,
                "mounted": mounted,
            }
        )

    return {
        "version": version,
        "select": select,
        "motor_on": motor_on,
        "motor_latch": motor_latch,
        "q6": q6,
        "q7": q7,
        "data_register": data_reg,
        "sequencer_state": seq,
        "mark_cycles_turnoff": mark_off,
        "cpu_cycles": cycles,
        "drives": drives,
    }


def fmt_line(st: dict, t_wall: float) -> str:
    d0 = st["drives"][0]
    d1 = st["drives"][1]
    ph = "".join(str(p) for p in d0["phases"])
    return (
        f"t={t_wall:7.3f}s cyc={st['cpu_cycles']:12d} "
        f"sel={st['select']} mot={st['motor_on']}/{st['motor_latch']} "
        f"q67={st['q6']}{st['q7']} "
        f"D0 track={d0['track']:>5s} ({d0['track_q']:3d}) ph={ph} en={d0['enable']} "
        f"D1 track={d1['track']:>5s} en={d1['enable']}"
    )


def sample_loop(
    c: Client,
    *,
    duration_s: float,
    interval_s: float,
    out_path: Path,
) -> int:
    t0 = time.monotonic()
    last_key = None
    changes = 0
    samples = 0
    track_hist: list[tuple[float, int, list[int]]] = []

    with out_path.open("w", encoding="utf-8") as out:
        while True:
            elapsed = time.monotonic() - t0
            if elapsed >= duration_s:
                break
            blob = c.state_get(DEVICE_ID_DISK_II)
            st = parse_diskii_state(blob)
            d0 = st["drives"][0]
            key = (d0["track_q"], tuple(d0["phases"]), st["select"], st["motor_on"])
            samples += 1
            if key != last_key:
                last_key = key
                changes += 1
                line = fmt_line(st, elapsed)
                print(line, flush=True)
                track_hist.append((elapsed, d0["track_q"], list(d0["phases"])))
                out.write(json.dumps({"t": elapsed, **st}) + "\n")
            time.sleep(interval_s)

    # Summarize thrash around quarter-tracks near 4.0
    if track_hist:
        tracks = [tq for _, tq, _ in track_hist]
        print(
            f"\n# samples={samples} changes={changes} "
            f"track_q range=[{min(tracks)},{max(tracks)}] "
            f"unique_tracks={sorted(set(tracks))}",
            flush=True,
        )
        # Count transitions involving 15..17 (3.75 / 4.0 / 4.25)
        near = {tq for tq in tracks if 14 <= tq <= 18}
        if near:
            print(f"# near-track-4 quarter indices seen: {sorted(near)}", flush=True)
            thrash = 0
            for i in range(1, len(track_hist)):
                a, b = track_hist[i - 1][1], track_hist[i][1]
                if {a, b} <= {14, 15, 16, 17, 18} and a != b:
                    thrash += 1
            print(f"# adjacent thrash transitions in 14..18: {thrash}", flush=True)
    return changes


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--sock", type=Path, default=None)
    ap.add_argument("--no-launch", action="store_true")
    ap.add_argument("--disk", type=Path, default=DEFAULT_DISK)
    ap.add_argument("--duration", type=float, default=45.0)
    ap.add_argument("--interval", type=float, default=0.02)
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("/tmp/gs2-a2osx-diskii.ndjson"),
    )
    ap.add_argument("--quit", action="store_true", help="QUIT emu when done")
    args = ap.parse_args()

    sock = args.sock or Path(f"/tmp/gs2-a2osx-{os.getpid()}.sock")
    proc = None
    if not args.no_launch:
        if sock.exists():
            sock.unlink()
        if not args.disk.is_file():
            raise FileNotFoundError(args.disk)
        gs2 = find_gs2()
        cmd = [
            str(gs2),
            "--debug",
            str(sock),
            "-p",
            "3",
            f"-ds6d1={args.disk}",
            "--no-quit-confirm",
        ]
        print("# launch:", " ".join(cmd), flush=True)
        proc = subprocess.Popen(
            cmd,
            cwd=str(REPO_ROOT),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True,
        )
        wait_for_socket(sock)
    else:
        wait_for_socket(sock)

    try:
        with Client() as c:
            c.connect(str(sock))
            c.hello()
            st = c.get_status()
            print(
                f"# platform={st.platform_id} mode={st.execution_mode} "
                f"(expect {PLATFORM_APPLE_IIE_ENHANCED})",
                flush=True,
            )
            blob = c.state_get(DEVICE_ID_DISK_II)
            assert len(blob) == DISKII_STATE_V1_SIZE, len(blob)
            parsed = parse_diskii_state(blob)
            assert parsed["version"] == 1
            print(
                f"# STATE_GET ok mounted={parsed['drives'][0]['mounted']} "
                f"track={parsed['drives'][0]['track']}",
                flush=True,
            )
            print(f"# sampling {args.duration}s every {args.interval}s → {args.out}", flush=True)
            sample_loop(
                c,
                duration_s=args.duration,
                interval_s=args.interval,
                out_path=args.out,
            )
            if args.quit or proc is not None:
                c.quit()
                print("# quit", flush=True)
    finally:
        if proc is not None:
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=2)
            if sock.exists() and not args.no_launch:
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
        ValueError,
    ) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
