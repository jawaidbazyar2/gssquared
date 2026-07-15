#!/usr/bin/env python3
"""IIe Enhanced: RESET, IO-write breakpoint on $C010, CONTINUE, QUIT.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/watch_c010_iie.py

Launches ``./build/GSSquared --debug <sock> -p 3``, waits 5s, warm RESET, arms
an IO write breakpoint on ``$C010`` (keyboard strobe), waits up to 10s for
``EVT_STOPPED`` (emu enters STEP_INTO), prints the stop packet, **holds 10s in
STEP** so the pause is visible, then ``CONTINUE``, waits 10s more while running,
then ``QUIT``.
"""

from __future__ import annotations

import os
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    PLATFORM_APPLE_IIE_ENHANCED,
    Client,
    ProtocolError,
)
from gs2debug.client import StoppedEvent
from gs2debug.types import (
    BP_ACCESS_W,
    BP_FLAG_ENABLED,
    BP_KIND_IO,
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


def format_stopped(st: StoppedEvent) -> str:
    reason = STOP_REASON_NAME.get(st.reason, f"reason={st.reason}")
    lines = [
        "EVT_STOPPED:",
        f"  reason          {reason} ({st.reason})",
        f"  bp_id           {st.bp_id}",
        f"  pc              ${st.pc:06X}",
        f"  eaddr           ${st.eaddr:06X}",
        f"  value           ${st.value:02X} ({st.value})",
        f"  access          {st.access}",
        f"  kind            {st.kind}",
        f"  execution_mode  {st.execution_mode}",
        f"  trace_bytes     {len(st.trace)}",
    ]
    if st.trace:
        lines.append(f"  trace           {st.trace.hex()}")
    return "\n".join(lines)


def main() -> int:
    gs2 = find_gs2()
    sock = Path(f"/tmp/gs2-c010-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    log_path = sock.with_suffix(".log")
    log_f = open(log_path, "w")
    proc = subprocess.Popen(
        [str(gs2), "--debug", str(sock), "-p", "3"],
        stdout=log_f,
        stderr=subprocess.STDOUT,
        cwd=str(REPO_ROOT),
        start_new_session=True,
    )
    print(f"launched {gs2} pid={proc.pid} -p 3 socket={sock} log={log_path}", flush=True)

    try:
        wait_for_socket(sock)
        with Client() as c:
            c.connect(str(sock))
            c.hello()
            status = c.get_status()
            if status.platform_id != PLATFORM_APPLE_IIE_ENHANCED:
                raise RuntimeError(
                    f"platform_id={status.platform_id}, "
                    f"expected IIe Enhanced ({PLATFORM_APPLE_IIE_ENHANCED})"
                )
            print(
                f"connected platform_id={status.platform_id} mode={status.execution_mode}",
                flush=True,
            )

            print("waiting 5s for boot...", flush=True)
            time.sleep(5.0)

            print("RESET (warm)...", flush=True)
            c.reset(cold_start=False)

            bp_id = c.bp_set(
                kind=BP_KIND_IO,
                address=0xC010,
                length=1,
                flags=BP_FLAG_ENABLED,
                access=BP_ACCESS_W,
            )
            print(f"IO write bp id={bp_id} $C010 — waiting up to 10s for stop...", flush=True)

            hit: StoppedEvent | None = None
            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline:
                try:
                    st = c.wait_stopped(timeout=max(0.05, deadline - time.monotonic()))
                except TimeoutError:
                    break
                if st.reason == STOP_BP_IO and st.bp_id == bp_id:
                    hit = st
                    break
                print(
                    f"  (ignoring stop reason={st.reason} bp_id={st.bp_id})",
                    flush=True,
                )

            if hit is None:
                print("WARNING: no $C010 write stop within 10s — continuing anyway", flush=True)
            else:
                print(format_stopped(hit), flush=True)
                status = c.get_status()
                print(
                    f"stopped in execution_mode={status.execution_mode} "
                    f"(expect STEP_INTO=1) — holding 10s before CONTINUE...",
                    flush=True,
                )
                time.sleep(10.0)

            print("CONTINUE...", flush=True)
            c.continue_()

            print("waiting 10s more (running)...", flush=True)
            time.sleep(10.0)

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
    except (OSError, ProtocolError, RuntimeError, TimeoutError, FileNotFoundError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
