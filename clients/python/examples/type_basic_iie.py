#!/usr/bin/env python3
"""Boot wait, protocol RESET, then type a short Applesoft program on IIe.

Usage:
  ./build/GSSquared --debug /tmp/gs2.sock -p 2
  PYTHONPATH=clients/python/src python3 clients/python/examples/type_basic_iie.py /tmp/gs2.sock
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import Client, ProtocolError

# HCOLOR=5 is orange on HGR. Circle via COS/SIN + HPLOT / HPLOT TO.
PROGRAM = """\
NEW
10 HGR
20 HCOLOR=5
30 CX=140:CY=96:R=70
40 A=0
50 X=INT(CX+R*COS(A)):Y=INT(CY+R*SIN(A)):HPLOT X,Y
60 FOR A=0.03 TO 6.28 STEP 0.03
70 X=INT(CX+R*COS(A)):Y=INT(CY+R*SIN(A)):HPLOT TO X,Y
80 NEXT
90 GOTO 90
RUN
"""


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
        print("RESET (warm)...")
        client.reset(cold_start=False)
        print("waiting 2s for BASIC prompt...")
        time.sleep(2.0)
        print(f"typing program:\n{PROGRAM}")
        client.type_text(PROGRAM, delay_s=0.1)
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
