#!/usr/bin/env python3
"""IIgs smoke: MEM_ENSONIQ DOC RAM + STATE_GET Ensoniq snapshot.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/test_ensoniq_debug.py

Launches ``./build/GSSquared --debug <sock> -p 5``, peeks DOC RAM, fetches
STATE_GET for DEVICE_ID_ENSONIQ, asserts v1 blob size, then QUITs.
"""

from __future__ import annotations

import os
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    DEVICE_ID_ENSONIQ,
    MEM_ENSONIQ,
    PLATFORM_APPLE_IIGS,
    Client,
    ProtocolError,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
ENSONIQ_STATE_V1_SIZE = 784


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


def main() -> int:
    sock = Path(f"/tmp/gs2-ensoniq-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    gs2 = find_gs2()
    proc = subprocess.Popen(
        [str(gs2), "--debug", str(sock), "-p", "5", "--no-quit-confirm"],
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

            doc = c.read_mem(MEM_ENSONIQ, 0, 16)
            assert len(doc) == 16, len(doc)
            print(f"MEM_ENSONIQ[0:16] = {doc.hex()}", flush=True)

            marker = bytes([0xDE, 0xAD, 0xBE, 0xEF])
            c.write_mem(MEM_ENSONIQ, 0x100, marker)
            back = c.read_mem(MEM_ENSONIQ, 0x100, 4)
            assert back == marker, back
            print("MEM_ENSONIQ poke/peek ok", flush=True)

            blob = c.state_get(DEVICE_ID_ENSONIQ)
            assert len(blob) == ENSONIQ_STATE_V1_SIZE, len(blob)
            (version,) = struct.unpack_from("<I", blob, 0)
            assert version == 1, version
            oscs = blob[10]
            rate = struct.unpack_from("<I", blob, 12)[0]
            print(
                f"STATE_GET ok version={version} len={len(blob)} "
                f"oscsenabled={oscs} rate={rate}",
                flush=True,
            )

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
    except (OSError, ProtocolError, RuntimeError, TimeoutError, AssertionError, FileNotFoundError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(1)
