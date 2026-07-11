#!/usr/bin/env python3
"""Write random bytes to Apple II text page 1 row 0 ($0400..$0427), read back, verify.

Each line shows the written pattern with '+' if readback matches, '-' if not.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 2
  PYTHONPATH=clients/python/src python3 clients/python/examples/write_text40.py /tmp/gs2.sock
"""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import MEM_MAIN, Client, ProtocolError

ADDR = 0x0400
LENGTH = 0x28  # $0400..$0427 inclusive


def _ascii_byte(b: int) -> str:
    c = b & 0x7F
    if 0x20 <= c <= 0x7E:
        return chr(c)
    return "."


def dump_line(ok: bool, data: bytes) -> str:
    mark = "+" if ok else "-"
    hex_part = " ".join(f"{b:02x}" for b in data)
    ascii_part = "".join(_ascii_byte(b) for b in data)
    return f"{mark} {ADDR:04x}: {hex_part}  |{ascii_part}|"


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} SOCKET_PATH", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with Client() as client:
        client.connect(path)
        info = client.hello()
        print(f"HELLO ok: version={info.version} max_payload={info.max_payload:#x}")
        print(
            f"writing MAIN ${ADDR:04X}..${ADDR + LENGTH - 1:04X} "
            f"every 1s with readback verify (Ctrl-C to stop)"
        )
        while True:
            data = os.urandom(LENGTH)
            client.write_mem(MEM_MAIN, ADDR, data)
            got = client.read_mem(MEM_MAIN, ADDR, LENGTH)
            print(dump_line(got == data, data), flush=True)
            time.sleep(1)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (OSError, ProtocolError, RuntimeError, TimeoutError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
