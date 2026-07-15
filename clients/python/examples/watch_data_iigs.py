#!/usr/bin/env python3
"""Launch IIgs, arm a DATA (R/W) breakpoint, RESET, print the stop packet, QUIT.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/watch_data_iigs.py \\
      ADDRESS [LENGTH]

  ADDRESS  24-bit linear CPU address. Forms accepted:
             0x020000   $020000   02/0000   (bank $02, offset $0000)
           This is NOT the same as Monitor ``2000`` (bank $00, offset $2000).
  LENGTH   byte span of the DATA watch [addr, addr+length). Default: 1.
           This is NOT an ignore/hit count — see --ignore.

  --ignore N   skip N matching accesses before stopping (0 = stop on first).
               To stop on the 5th read of one byte: LENGTH=1 --ignore 4.

Launches ``./build/GSSquared --debug <sock> -p 5``, waits for boot, sets a
read/write DATA breakpoint, sends warm RESET, waits for EVT_STOPPED, prints the
stop packet, then ``QUIT``.

Guest firmware rarely touches arbitrary banks after RESET. Trigger the watch from
the IIgs Monitor with a *banked* examine, e.g. ``02/0000`` (not ``2000``), or
pass ``--demo-stub`` to plant a tiny LDA-long loop at $0300 and ``300G``.
"""

from __future__ import annotations

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    PLATFORM_APPLE_IIGS,
    Client,
    ProtocolError,
)
from gs2debug.client import StoppedEvent
from gs2debug.types import (
    BP_ACCESS_RW,
    BP_FLAG_ENABLED,
    BP_KIND_DATA,
    STOP_BP_DATA,
    STOP_BP_EXEC,
    STOP_BP_IO,
    STOP_PAUSE,
    STOP_STEP,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"

STOP_REASON_NAME = {
    STOP_BP_EXEC: "STOP_BP_EXEC",
    STOP_BP_DATA: "STOP_BP_DATA",
    STOP_BP_IO: "STOP_BP_IO",
    STOP_STEP: "STOP_STEP",
    STOP_PAUSE: "STOP_PAUSE",
}

ACCESS_NAME = {
    0: "NONE",
    1: "R",
    2: "W",
    3: "RW",
}

KIND_NAME = {
    1: "EXEC",
    2: "DATA",
    3: "IO",
}


def parse_address(text: str) -> int:
    """Parse 0xHHLLLL, $HHLLLL, HH/LLLL, or decimal into a 24-bit linear address."""
    s = text.strip()
    if "/" in s:
        bank_s, off_s = s.split("/", 1)
        bank = int(bank_s.strip().lstrip("$"), 16)
        off = int(off_s.strip().lstrip("$"), 16)
        if bank < 0 or bank > 0xFF or off < 0 or off > 0xFFFF:
            raise ValueError(f"bad bank/offset address: {text!r}")
        return (bank << 16) | off
    if s.startswith("$"):
        s = "0x" + s[1:]
    return int(s, 0)


def format_bank_offset(addr: int) -> str:
    return f"{(addr >> 16) & 0xFF:02X}/{addr & 0xFFFF:04X}"


def find_gs2(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit)
    else:
        env = os.environ.get("GS2_BIN")
        path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


def wait_for_socket(path: Path, timeout_s: float = 30.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if path.is_socket() or path.exists():
            try:
                with Client() as probe:
                    probe.connect(str(path))
                return
            except OSError:
                pass
        time.sleep(0.1)
    raise TimeoutError(f"debug socket not ready: {path}")


def format_stopped(st: StoppedEvent) -> str:
    reason = STOP_REASON_NAME.get(st.reason, f"reason={st.reason}")
    access = ACCESS_NAME.get(st.access, str(st.access))
    kind = KIND_NAME.get(st.kind, str(st.kind))
    lines = [
        "EVT_STOPPED:",
        f"  reason          {reason} ({st.reason})",
        f"  bp_id           {st.bp_id}",
        f"  pc              ${st.pc:06X}",
        f"  eaddr           ${st.eaddr:06X}  ({format_bank_offset(st.eaddr)})",
        f"  value           ${st.value:02X} ({st.value})",
        f"  access          {access} ({st.access})",
        f"  kind            {kind} ({st.kind})",
        f"  execution_mode  {st.execution_mode}",
        f"  trace_bytes     {len(st.trace)}",
    ]
    if st.trace:
        lines.append(f"  trace           {st.trace.hex()}")
    return "\n".join(lines)


def run_demo_stub(c: Client, address: int) -> None:
    """Plant LDA long / BRA * at $0300 and type 300G from Applesoft via Monitor."""
    # AF ll hh bb  LDA al addr ; 80 FE BRA *
    stub = bytes(
        [
            0xAF,
            address & 0xFF,
            (address >> 8) & 0xFF,
            (address >> 16) & 0xFF,
            0x80,
            0xFE,
        ]
    )
    c.write_mem(0, 0x0300, stub)
    print("demo: wrote LDA-long stub at $0300; entering Monitor and 300G", flush=True)
    c.type_text("CALL -151\n", delay_s=0.15, hold_s=0.03)
    time.sleep(0.5)
    c.type_text("300G\n", delay_s=0.15, hold_s=0.03)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "address",
        type=parse_address,
        help="DATA watch base (0x020000, $020000, or 02/0000)",
    )
    ap.add_argument(
        "length",
        nargs="?",
        type=int,
        default=1,
        help="watch length in bytes (default: 1). Not an ignore count.",
    )
    ap.add_argument(
        "--ignore",
        type=int,
        default=0,
        metavar="N",
        help="skip N hits before stopping (default: 0 = first hit)",
    )
    ap.add_argument(
        "--gs2",
        default=None,
        help=f"path to GSSquared (default: {DEFAULT_GS2} or $GS2_BIN)",
    )
    ap.add_argument(
        "--socket",
        default=None,
        help="Unix socket path (default: /tmp/gs2-watch-<pid>.sock)",
    )
    ap.add_argument(
        "--boot-wait",
        type=float,
        default=5.0,
        help="seconds to wait after connect before RESET (default: 5)",
    )
    ap.add_argument(
        "--hit-timeout",
        type=float,
        default=60.0,
        help="seconds to wait for DATA stop after RESET (default: 60)",
    )
    ap.add_argument(
        "--demo-stub",
        action="store_true",
        help="after RESET, run a tiny LDA-long stub at $0300 that touches ADDRESS",
    )
    args = ap.parse_args()

    if args.length < 1:
        print("length must be >= 1", file=sys.stderr)
        return 2
    if args.ignore < 0:
        print("--ignore must be >= 0", file=sys.stderr)
        return 2

    gs2 = find_gs2(args.gs2)
    sock = Path(args.socket) if args.socket else Path(f"/tmp/gs2-watch-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    log_path = sock.with_suffix(".log")
    log_f = open(log_path, "w")
    proc = subprocess.Popen(
        [str(gs2), "--debug", str(sock), "-p", "5"],
        stdout=log_f,
        stderr=subprocess.STDOUT,
        cwd=str(REPO_ROOT),
        start_new_session=True,
    )
    print(f"launched {gs2} pid={proc.pid} socket={sock} log={log_path}", flush=True)

    try:
        wait_for_socket(sock)
        with Client() as c:
            c.connect(str(sock))
            c.hello()
            status = c.get_status()
            if status.platform_id != PLATFORM_APPLE_IIGS:
                raise RuntimeError(
                    f"platform_id={status.platform_id}, expected IIgs ({PLATFORM_APPLE_IIGS})"
                )
            print(
                f"connected platform_id={status.platform_id} mode={status.execution_mode}",
                flush=True,
            )
            print(f"waiting {args.boot_wait:g}s for boot...", flush=True)
            time.sleep(args.boot_wait)

            bp_id = c.bp_set(
                kind=BP_KIND_DATA,
                address=args.address,
                length=args.length,
                flags=BP_FLAG_ENABLED,
                access=BP_ACCESS_RW,
                ignore_count=args.ignore,
            )
            end = args.address + args.length
            print(
                f"DATA R/W bp id={bp_id} "
                f"[{args.address:#x}, {end:#x}) = {format_bank_offset(args.address)}"
                f"..{format_bank_offset(end - 1)}  ignore={args.ignore}",
                flush=True,
            )
            print(
                f"hint: in the IIgs Monitor, examine with "
                f"{format_bank_offset(args.address)}  (not a 4-digit bank-0 address)",
                flush=True,
            )

            print("RESET (warm)...", flush=True)
            c.reset(cold_start=False)

            if args.demo_stub:
                time.sleep(2.0)
                run_demo_stub(c, args.address)
            else:
                print(
                    f"waiting up to {args.hit_timeout:g}s for a CPU access in range "
                    f"(Monitor: {format_bank_offset(args.address)})...",
                    flush=True,
                )

            deadline = time.monotonic() + args.hit_timeout
            hit: StoppedEvent | None = None
            while time.monotonic() < deadline:
                try:
                    st = c.wait_stopped(timeout=max(0.05, deadline - time.monotonic()))
                except TimeoutError:
                    break
                if st.reason == STOP_BP_DATA and st.bp_id == bp_id:
                    hit = st
                    break
                print(
                    f"  (ignoring stop reason={st.reason} bp_id={st.bp_id})",
                    flush=True,
                )

            if hit is None:
                listed = c.bp_list()
                info = next((b for b in listed if b.id == bp_id), None)
                hits = info.hit_count if info else "?"
                raise TimeoutError(
                    f"DATA breakpoint id={bp_id} at {args.address:#x} "
                    f"({format_bank_offset(args.address)}) did not stop "
                    f"(hit_count={hits}, ignore was {args.ignore}). "
                    f"CPU must touch that 24-bit address — Monitor '2000' is "
                    f"$00/2000, not $02/0000. Try --demo-stub to prove the watch."
                )

            print(format_stopped(hit), flush=True)

            try:
                c.quit()
                print("quit ok", flush=True)
            except (OSError, ConnectionError, ProtocolError) as exc:
                print(f"quit socket closed: {exc}", flush=True)
    except BaseException:
        if proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        raise
    finally:
        if proc.poll() is None:
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                print("emu still running after QUIT — SIGKILL", flush=True)
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                proc.wait(timeout=5)
        log_f.close()
        if sock.exists():
            try:
                sock.unlink()
            except OSError:
                pass

    rc = proc.returncode if proc.returncode is not None else -1
    print(f"emu exit={rc}", flush=True)
    return 0 if rc == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (OSError, ProtocolError, RuntimeError, TimeoutError, FileNotFoundError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
