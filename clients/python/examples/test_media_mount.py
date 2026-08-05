#!/usr/bin/env python3
"""Smoke MOUNT / UNMOUNT on Disk II (slot 6, unit 0).

  ./build/GSSquared --debug /tmp/gs2-media.sock -p 3 --no-quit-confirm
  PYTHONPATH=clients/python/src python3 clients/python/examples/test_media_mount.py /tmp/gs2-media.sock
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    MEDIA_BAD_PATH,
    MEDIA_MOUNT_FAILED,
    MEDIA_NO_DRIVE,
    MEDIA_OK,
    Client,
)

REPO = Path(__file__).resolve().parents[3]
DEFAULT_DISK = REPO / "disk_images" / "SPFBase.dsk"


def main() -> int:
    sock = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gs2-media.sock"
    disk = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else DEFAULT_DISK.resolve()
    if not disk.is_file():
        print(f"missing disk image: {disk}", file=sys.stderr)
        return 1

    with Client() as c:
        c.connect(sock)
        c.hello()

        # Bad slot → no drive registered
        st = c.mount(0, 0, str(disk))
        assert st == MEDIA_NO_DRIVE, st
        print("  mount no-drive ok")

        st = c.mount(6, 0, "")
        assert st == MEDIA_BAD_PATH, st
        print("  mount bad-path ok")

        st = c.mount(6, 0, str(disk))
        assert st == MEDIA_OK, st
        print(f"  mount slot6 unit0 ok ({disk})")

        st = c.mount(6, 0, str(Path("/nonexistent/no-such-disk.dsk")))
        assert st == MEDIA_MOUNT_FAILED, st
        print("  mount failed (missing file) ok")

        st = c.unmount(6, 0)
        assert st == MEDIA_OK, st
        print("  unmount ok")

        st = c.unmount(0, 0)
        assert st == MEDIA_NO_DRIVE, st
        print("  unmount no-drive ok")

        c.quit()
    print("all ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
