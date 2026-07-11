#!/usr/bin/env python3
"""Dump IIgs text page 1 row 0 in bank $E0 ($E0/0400..$E0/0427) once per second.

Intended for Apple IIgs (-p 5). Uses MAIN-domain READMEM at linear address 0xE00400.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 5
  PYTHONPATH=clients/python/src python3 clients/python/examples/read_text40_iigs.py /tmp/gs2.sock
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import MEM_MAIN, Client, ProtocolError

BANK = 0xE0
OFFSET = 0x0400
ADDR = (BANK << 16) | OFFSET  # $E0/0400
LENGTH = 0x28  # $E0/0400..$E0/0427 inclusive


def _ascii_byte(b: int) -> str:
    c = b & 0x7F
    if 0x20 <= c <= 0x7E:
        return chr(c)
    return "."


def dump_line(data: bytes) -> str:
    hex_part = " ".join(f"{b:02x}" for b in data)
    ascii_part = "".join(_ascii_byte(b) for b in data)
    return f"{BANK:02x}/{OFFSET:04x}: {hex_part}  |{ascii_part}|"


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} SOCKET_PATH", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with Client() as client:
        client.connect(path)
        info = client.hello()
        print(f"HELLO ok: version={info.version} max_payload={info.max_payload:#x}")
        end = OFFSET + LENGTH - 1
        print(
            f"reading MAIN ${BANK:02X}/{OFFSET:04X}..${BANK:02X}/{end:04X} "
            f"(addr={ADDR:#08x}) every 1s (Ctrl-C to stop)"
        )
        while True:
            data = client.read_mem(MEM_MAIN, ADDR, LENGTH)
            print(dump_line(data), flush=True)
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
