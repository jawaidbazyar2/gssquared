#!/usr/bin/env python3
"""Boot wait, Control+Reset (Ctrl+F12), then type a short Applesoft program on IIe.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 1
  PYTHONPATH=clients/python/src python3 clients/python/examples/type_basic_iie.py /tmp/gs2.sock
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    KMOD_CTRL,
    KMOD_LCTRL,
    SCANCODE_F12,
    SCANCODE_LCTRL,
    Client,
    ProtocolError,
)

PROGRAM = '10 PRINT "BOOGERS"\n20 END\nRUN\n'


def control_reset(client: Client) -> None:
    """Hold Control, press F12 (Reset), release — matches IIe keyboard.cpp."""
    client.key_down(SCANCODE_LCTRL, KMOD_LCTRL)
    client.key_down(SCANCODE_F12, KMOD_CTRL)
    time.sleep(1.0)
    client.key_up(SCANCODE_F12, KMOD_CTRL)
    client.key_up(SCANCODE_LCTRL, 0)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} SOCKET_PATH", file=sys.stderr)
        return 2

    path = sys.argv[1]
    with Client() as client:
        client.connect(path)
        info = client.hello()
        print(f"HELLO ok: version={info.version} max_payload={info.max_payload:#x}")
        print("waiting 5s for boot...")
        time.sleep(5.0)
        print("Control+Reset (Ctrl+F12)...")
        control_reset(client)
        print("waiting 2s for BASIC prompt...")
        time.sleep(2.0)
        print(f"typing program:\n{PROGRAM}")
        client.type_text(PROGRAM, delay_s=0.08)
        print("done")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (OSError, ProtocolError, RuntimeError, TimeoutError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
