#!/usr/bin/env python3
"""Type text / BASIC into a running GSSquared via the debug KEYEVENT API.

Meant to be called as a tool from scripts or agents.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 1

  # Type a string (Return appended unless --no-return)
  PYTHONPATH=clients/python/src python3 clients/python/examples/type_to_emu.py /tmp/gs2.sock \\
    --text 'PRINT "HI"'

  # Type a multi-line program from stdin
  PYTHONPATH=clients/python/src python3 clients/python/examples/type_to_emu.py /tmp/gs2.sock <<'EOF'
10 GR
20 END
RUN
EOF

  # Control+Reset first, then type
  PYTHONPATH=clients/python/src python3 clients/python/examples/type_to_emu.py /tmp/gs2.sock \\
    --reset --wait 2 --file program.bas
"""

from __future__ import annotations

import argparse
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


def control_reset(client: Client) -> None:
    client.key_down(SCANCODE_LCTRL, KMOD_LCTRL)
    client.key_down(SCANCODE_F12, KMOD_CTRL)
    time.sleep(1.0)
    client.key_up(SCANCODE_F12, KMOD_CTRL)
    client.key_up(SCANCODE_LCTRL, 0)


def main() -> int:
    p = argparse.ArgumentParser(description="Type text into GSSquared over the debug socket")
    p.add_argument("socket", help="Unix-domain debug socket path (e.g. /tmp/gs2.sock)")
    p.add_argument("--text", "-t", action="append", default=[], help="Text to type (repeatable)")
    p.add_argument("--file", "-f", help="Read text from file")
    p.add_argument("--reset", action="store_true", help="Control+Reset (Ctrl+F12) before typing")
    p.add_argument("--wait", type=float, default=0.0, help="Seconds to sleep after connect/reset")
    p.add_argument("--delay", type=float, default=0.06, help="Delay between keystrokes (seconds)")
    p.add_argument(
        "--no-return",
        action="store_true",
        help="Do not append Return after each --text argument",
    )
    args = p.parse_args()

    chunks: list[str] = []
    for t in args.text:
        chunks.append(t if args.no_return or t.endswith("\n") else t + "\n")
    if args.file:
        chunks.append(Path(args.file).read_text())
    if not chunks and not sys.stdin.isatty():
        chunks.append(sys.stdin.read())
    if not chunks and not args.reset:
        p.error("provide --text, --file, or stdin (or --reset alone)")

    body = "".join(chunks)

    with Client() as client:
        client.connect(args.socket)
        info = client.hello()
        print(f"HELLO ok: version={info.version} max_payload={info.max_payload:#x}", flush=True)
        if args.reset:
            print("Control+Reset...", flush=True)
            control_reset(client)
        if args.wait > 0:
            print(f"waiting {args.wait}s...", flush=True)
            time.sleep(args.wait)
        if body:
            print(f"typing {len(body)} chars...", flush=True)
            client.type_text(body, delay_s=args.delay)
        print("done", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except (OSError, ProtocolError, RuntimeError, TimeoutError, ValueError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
