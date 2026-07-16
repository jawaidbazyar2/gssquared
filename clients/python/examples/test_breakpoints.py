#!/usr/bin/env python3
"""Exercise breakpoint protocol on a running emulator.

Run twice (separate emu instances / sockets):

  # IIe Enhanced
  ./build/GSSquared --debug /tmp/gs2-iie.sock -p 3
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_breakpoints.py /tmp/gs2-iie.sock 3

  # Apple IIgs
  ./build/GSSquared --debug /tmp/gs2-gs.sock -p 5
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_breakpoints.py /tmp/gs2-gs.sock 5
"""

from __future__ import annotations

import struct
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
    BP_KIND_EXEC,
    BP_KIND_IO,
    EVT_RUN_STATE,
    EXEC_NORMAL,
    EXEC_PAUSED,
    EXEC_STEP_INTO,
    STOP_BP_EXEC,
    STOP_BP_IO,
    STOP_PAUSE,
    STOP_STEP,
)


def drain_events(c: Client, seconds: float = 0.2) -> None:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            c.wait_event(timeout=max(0.01, deadline - time.monotonic()))
        except TimeoutError:
            break
        except (ProtocolError, OSError, ConnectionError):
            # Leftover non-EVENT frames or a closing socket — stop draining.
            break


def test_pause_continue(c: Client) -> None:
    c.pause()
    st = c.wait_stopped(timeout=2.0)
    assert st.reason == STOP_PAUSE, st
    assert st.execution_mode == EXEC_PAUSED, st
    c.continue_()
    for _ in range(8):
        try:
            eid, _s, data = c.wait_event(timeout=1.0)
        except TimeoutError:
            break
        if eid == EVT_RUN_STATE and len(data) >= 4:
            (mode,) = struct.unpack_from("<I", data, 0)
            if mode == EXEC_NORMAL:
                break
    status = c.get_status()
    assert status.execution_mode == EXEC_NORMAL, status
    print("  pause/continue ok")


def test_exec_bp(c: Client) -> None:
    c.bp_clear_all()
    c.pause()
    paused = c.wait_stopped(timeout=2.0)
    assert paused.reason == STOP_PAUSE
    addr = paused.pc
    bp_id = c.bp_set(kind=BP_KIND_EXEC, address=addr, length=1, flags=BP_FLAG_ENABLED)
    assert bp_id != 0
    listed = c.bp_list()
    assert any(b.id == bp_id for b in listed), listed

    # CONTINUE from PAUSE does not arm Policy A suppress; EXEC at current PC should fire.
    c.continue_()
    hit: StoppedEvent | None = None
    deadline = time.monotonic() + 3.0
    while time.monotonic() < deadline:
        try:
            st = c.wait_stopped(timeout=deadline - time.monotonic())
        except TimeoutError:
            break
        if st.reason == STOP_BP_EXEC and st.bp_id == bp_id:
            hit = st
            break
    if hit is None:
        raise RuntimeError(f"EXEC breakpoint at ${addr:06X} did not fire")
    assert hit.execution_mode == EXEC_STEP_INTO
    print(f"  EXEC hit id={bp_id} pc=${hit.pc:06X}")

    # Policy A: continue from EXEC stop should leave the bp without immediate re-hit wedge
    c.continue_()
    time.sleep(0.1)
    status = c.get_status()
    assert status.execution_mode in (EXEC_NORMAL, EXEC_STEP_INTO), status
    print("  EXEC Policy A continue ok")

    c.bp_clear(bp_id)
    assert all(b.id != bp_id for b in c.bp_list())


def test_io_bp(c: Client, platform_id: int) -> None:
    c.bp_clear_all()
    bp_id = c.bp_set(
        kind=BP_KIND_IO,
        address=0xC030,
        length=1,
        flags=BP_FLAG_ENABLED,
        access=BP_ACCESS_RW,
    )
    c.continue_()
    hit = None
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        try:
            st = c.wait_stopped(timeout=deadline - time.monotonic())
        except TimeoutError:
            break
        if st.reason == STOP_BP_IO and st.bp_id == bp_id:
            hit = st
            break
    if hit is None:
        print("  IO bp set/list (no hit within timeout — ok)")
    else:
        bank = (hit.eaddr >> 16) & 0xFF
        if platform_id == PLATFORM_APPLE_IIGS:
            assert bank in (0x00, 0x01, 0xE0, 0xE1), hex(hit.eaddr)
        print(f"  IO hit eaddr=${hit.eaddr:06X}")
    c.bp_clear(bp_id)


def test_step_into(c: Client) -> None:
    c.bp_clear_all()
    c.pause()
    paused = c.wait_stopped(timeout=2.0)
    assert paused.reason == STOP_PAUSE
    c.step_into(1)
    stepped = c.wait_stopped(timeout=3.0)
    assert stepped.reason == STOP_STEP, stepped
    assert stepped.execution_mode == EXEC_STEP_INTO, stepped
    assert len(stepped.trace) == 40, len(stepped.trace)
    print(f"  STEP_INTO ok pc=${stepped.pc:06X} trace={len(stepped.trace)}B")
    c.continue_()
    drain_events(c, 0.2)


def test_reset_keeps_bps(c: Client) -> None:
    c.bp_clear_all()
    bp_id = c.bp_set(kind=BP_KIND_EXEC, address=0xFF69, length=1)
    c.reset(cold_start=False)
    time.sleep(0.2)
    drain_events(c, 0.3)
    ids = [b.id for b in c.bp_list()]
    assert bp_id in ids, ids
    c.bp_clear_all()
    print("  RESET keeps breakpoints ok")


def test_cap(c: Client) -> None:
    c.bp_clear_all()
    try:
        for i in range(257):
            # Keep disabled so the hot path stays cheap while filling the table.
            c.bp_set(kind=BP_KIND_EXEC, address=0x1000 + i, length=1, flags=0)
        raise RuntimeError("expected E_BAD_LENGTH at 257th breakpoint")
    except ProtocolError as exc:
        if exc.code != 2:  # E_BAD_LENGTH
            raise
        print("  cap 256 ok")
    finally:
        try:
            c.bp_clear_all()
        except OSError:
            pass


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} SOCKET_PATH PLATFORM_ID", file=sys.stderr)
        return 2
    path = sys.argv[1]
    want_platform = int(sys.argv[2])

    with Client() as c:
        c.connect(path)
        c.hello()
        status = c.get_status()
        if status.platform_id != want_platform:
            raise RuntimeError(
                f"platform_id={status.platform_id}, expected {want_platform}"
            )
        print(f"platform_id={status.platform_id} mode={status.execution_mode}", flush=True)

        test_pause_continue(c)
        test_exec_bp(c)
        test_io_bp(c, want_platform)
        test_step_into(c)
        test_reset_keeps_bps(c)
        test_cap(c)
        print("PASS", flush=True)
        drain_events(c, 0.3)
        try:
            c.quit()
            print("quit ok", flush=True)
        except (OSError, ConnectionError, ProtocolError) as exc:
            # Socket may already be closing as the emu exits.
            print(f"quit socket closed: {exc}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (OSError, ProtocolError, RuntimeError, TimeoutError, AssertionError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
