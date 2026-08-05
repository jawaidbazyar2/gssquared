#!/usr/bin/env python3
"""Smoke GET_REGS / SET_REGS / FINDMEM on a running emulator.

  ./build/GSSquared --debug /tmp/gs2-regs.sock -p 3 --no-quit-confirm
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_regs_findmem.py /tmp/gs2-regs.sock
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    MEM_MAIN,
    REG_A,
    Client,
    STOP_PAUSE,
)


def main() -> int:
    sock = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gs2-regs.sock"
    with Client() as c:
        c.connect(sock)
        c.hello()
        c.pause()
        st = c.wait_stopped(timeout=2.0)
        assert st.reason == STOP_PAUSE, st

        regs = c.get_regs()
        assert len(regs) == 40, len(regs)
        a0 = struct.unpack_from("<H", regs, 18)[0]
        print(f"  get_regs ok (a=${a0:04X})")

        c.set_regs(REG_A, a=0x1234)
        a1 = struct.unpack_from("<H", c.get_regs(), 18)[0]
        assert a1 == 0x1234, f"expected a=1234 got {a1:04X}"
        print("  set_regs REG_A ok")

        # Restore prior A so we don't leave the machine weird for interactive use.
        c.set_regs(REG_A, a=a0)

        sig = b"GS2FIND"
        addr = 0x300
        c.write_mem(MEM_MAIN, addr, sig)
        hits = c.find_mem(MEM_MAIN, 0x0000, 0x1000, sig)
        assert addr in hits, hits
        print(f"  find_mem exact ok ({hits!r})")

        wild = c.find_mem(
            MEM_MAIN,
            0x0000,
            0x1000,
            b"GS2XXXX",
            mask=b"\xff\xff\xff\x00\x00\x00\x00",
        )
        assert addr in wild, wild
        print(f"  find_mem masked ok ({wild!r})")

        miss = c.find_mem(MEM_MAIN, 0x0000, 0x1000, b"NOMATCH!")
        assert miss == [], miss
        print("  find_mem miss ok")

        c.quit()
    print("all ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
