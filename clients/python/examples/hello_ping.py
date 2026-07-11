#!/usr/bin/env python3
"""HELLO / GET_STATUS / PING smoke test against a running GSSquared debug socket.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 1
  PYTHONPATH=clients/python/src python3 clients/python/examples/hello_ping.py /tmp/gs2.sock
"""

from __future__ import annotations

import sys
from pathlib import Path

# Allow running without install: PYTHONPATH=src or sibling layout
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import Client, ProtocolError

_MODE_NAMES = {
    0: "NORMAL",
    1: "STEP_INTO",
    2: "PAUSED",
}


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} SOCKET_PATH", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with Client() as client:
        client.connect(path)
        info = client.hello()
        print(f"HELLO ok: version={info.version} flags={info.flags} max_payload={info.max_payload:#x}")
        mode = client.get_status()
        name = _MODE_NAMES.get(mode, "UNKNOWN")
        print(f"GET_STATUS ok: execution_mode={mode} ({name})")
        client.ping()
        print("PING ok")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ProtocolError, RuntimeError, TimeoutError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
