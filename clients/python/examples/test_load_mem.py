#!/usr/bin/env python3
"""Smoke LOADMEM on a running emulator (IIe Enhanced -p 3 or IIgs -p 5).

  ./build/GSSquared --debug /tmp/gs2-loadmem.sock -p 3 --no-quit-confirm
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_load_mem.py /tmp/gs2-loadmem.sock
"""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    MEM_ENSONIQ,
    MEM_MAIN,
    MEM_MAIN_RAW,
    MEM_MEGAII,
    PLATFORM_APPLE_IIGS,
    STOP_PAUSE,
    Client,
    ProtocolError,
)
from gs2debug.types import E_BAD_LENGTH, E_INTERNAL


def expect_error(fn, expected_code: int, label: str) -> None:
    try:
        fn()
    except ProtocolError as exc:
        assert exc.code == expected_code, f"{label}: got code {exc.code} ({exc.message!r}), expected {expected_code}"
        print(f"  {label} ok (code={exc.code}, msg={exc.message!r})")
        return
    raise AssertionError(f"{label}: expected ProtocolError({expected_code}), succeeded")


def main() -> int:
    sock = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gs2-loadmem.sock"
    with tempfile.TemporaryDirectory(prefix="gs2-loadmem-") as tmpdir:
        tmp = Path(tmpdir)
        blob = bytes((i * 37 + 11) & 0xFF for i in range(4096))
        bin_path = tmp / "payload.bin"
        bin_path.write_bytes(blob)

        empty_path = tmp / "empty.bin"
        empty_path.write_bytes(b"")

        with Client() as c:
            c.connect(sock)
            c.hello()
            status = c.get_status()
            is_iigs = status.platform_id >= PLATFORM_APPLE_IIGS

            c.pause()
            st = c.wait_stopped(timeout=2.0)
            assert st.reason == STOP_PAUSE, st

            # 1. Whole file into MEM_MAIN at $2000
            n = c.load_mem(MEM_MAIN, 0x2000, str(bin_path))
            assert n == len(blob), f"expected {len(blob)}, got {n}"
            got = c.read_mem(MEM_MAIN, 0x2000, len(blob))
            assert got == blob, "whole-file readback mismatch"
            print(f"  load_mem whole file ({n} bytes) ok")

            # 2. Slice (file_offset + max_length) into MEM_MAIN at $3000
            n = c.load_mem(MEM_MAIN, 0x3000, str(bin_path), file_offset=512, max_length=256)
            assert n == 256, n
            got = c.read_mem(MEM_MAIN, 0x3000, 256)
            assert got == blob[512:768], "slice readback mismatch"
            print("  load_mem slice ok")

            # 3. max_length larger than remaining bytes clamps to EOF
            n = c.load_mem(MEM_MAIN, 0x3200, str(bin_path), file_offset=4000, max_length=500)
            assert n == 96, n
            got = c.read_mem(MEM_MAIN, 0x3200, 96)
            assert got == blob[4000:], "clamped slice readback mismatch"
            print("  load_mem EOF clamp ok")

            # 4. Empty file loads 0 bytes cleanly
            n = c.load_mem(MEM_MAIN, 0x3400, str(empty_path))
            assert n == 0, n
            print("  load_mem empty file ok")

            # 5. MAIN_RAW load + readback
            raw_addr = 0x20000 if is_iigs else 0x4000
            n = c.load_mem(MEM_MAIN_RAW, raw_addr, str(bin_path), file_offset=100, max_length=128)
            assert n == 128, n
            got = c.read_mem(MEM_MAIN_RAW, raw_addr, 128)
            assert got == blob[100:228], "MAIN_RAW readback mismatch"
            print(f"  load_mem MAIN_RAW @ 0x{raw_addr:X} ok")

            # 6. IIgs-specific domains or non-IIgs rejection
            if is_iigs:
                n = c.load_mem(MEM_MEGAII, 0x4000, str(bin_path), file_offset=0, max_length=64)
                assert n == 64, n
                assert c.read_mem(MEM_MEGAII, 0x4000, 64) == blob[:64]
                print("  load_mem MEGAII ok")

                n = c.load_mem(MEM_ENSONIQ, 0x0100, str(bin_path), file_offset=0, max_length=64)
                assert n == 64, n
                assert c.read_mem(MEM_ENSONIQ, 0x0100, 64) == blob[:64]
                print("  load_mem ENSONIQ ok")
            else:
                expect_error(
                    lambda: c.load_mem(MEM_MEGAII, 0x2000, str(bin_path), max_length=16),
                    E_INTERNAL,
                    "MEGAII on non-IIgs",
                )
                expect_error(
                    lambda: c.load_mem(MEM_MAIN, 0xFFF0, str(bin_path), max_length=32),
                    E_BAD_LENGTH,
                    "MAIN out-of-range",
                )

            # 7. File & bounds error paths
            expect_error(
                lambda: c.load_mem(MEM_MAIN, 0x2000, str(tmp / "missing.bin")),
                E_INTERNAL,
                "missing file",
            )
            expect_error(
                lambda: c.load_mem(MEM_MAIN, 0x2000, str(tmp)),
                E_INTERNAL,
                "directory path",
            )
            expect_error(
                lambda: c.load_mem(MEM_MAIN, 0x2000, str(bin_path), file_offset=len(blob) + 1),
                E_BAD_LENGTH,
                "file_offset past EOF",
            )
            expect_error(
                lambda: c.load_mem(MEM_MAIN_RAW, 0x7FFFFF00, str(bin_path), max_length=512),
                E_BAD_LENGTH,
                "MAIN_RAW out-of-range",
            )

            c.quit()
    print("all ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
