#!/usr/bin/env python3
"""Smoke VIDEO_TEXT on a running emulator.

  ./build/GSSquared --debug /tmp/gs2-video.sock -p 3 --no-quit-confirm
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_video_text.py /tmp/gs2-video.sock
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    MEM_MAIN,
    VIDEO_MODE_TEXT40,
    VIDEO_MODE_TEXT80,
    Client,
    ProtocolError,
    STOP_PAUSE,
)


def main() -> int:
    sock = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gs2-video.sock"
    with Client() as c:
        c.connect(sock)
        c.hello()
        # Let ROM settle so text page has something recognizable.
        time.sleep(1.5)
        c.pause()
        st = c.wait_stopped(timeout=2.0)
        assert st.reason == STOP_PAUSE, st

        vt = c.video_text()
        assert vt.page in (1, 2), vt
        assert vt.mode in (VIDEO_MODE_TEXT40, VIDEO_MODE_TEXT80), vt
        assert vt.rows == 24, vt
        assert vt.cols in (40, 80), vt
        assert len(vt.chars) == vt.cols * vt.rows, len(vt.chars)
        lines = vt.as_lines()
        assert len(lines) == 24, len(lines)
        print(f"  CURRENT -> page={vt.page} mode={vt.mode} flags=0x{vt.flags:x}")
        print("  --- screen ---")
        for line in lines:
            print(f"  |{line}|")
        print("  --------------")

        vt40 = c.video_text(1, VIDEO_MODE_TEXT40)
        assert vt40.page == 1 and vt40.mode == VIDEO_MODE_TEXT40
        assert vt40.cols == 40 and len(vt40.chars) == 960
        # Spot-check de-skew vs raw nonlinear line 0 / line 8.
        raw0 = c.read_mem(MEM_MAIN, 0x400, 40)
        raw8 = c.read_mem(MEM_MAIN, 0x428, 40)
        assert vt40.chars[0:40] == raw0, (vt40.chars[0:40], raw0)
        assert vt40.chars[8 * 40 : 9 * 40] == raw8
        print("  TEXT40 page1 de-skew ok")

        try:
            vt80 = c.video_text(1, VIDEO_MODE_TEXT80)
            assert vt80.mode == VIDEO_MODE_TEXT80 and vt80.cols == 80
            assert len(vt80.chars) == 1920
            print("  TEXT80 page1 ok")
        except ProtocolError as e:
            print(f"  TEXT80 unavailable ({e.message})")

        c.quit()
    print("all ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
