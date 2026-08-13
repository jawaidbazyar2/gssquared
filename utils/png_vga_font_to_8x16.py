#!/usr/bin/env python3
"""Convert an IBM VGA 8x16 PNG atlas (9x17 cell stride) to a 4096-byte 8x16 font bin.

Atlas layout matches GSSquared vga_render_text_9x16:
  ATLAS_COL_STRIDE = 9, ATLAS_ROW_STRIDE = 17
  16 columns x 16 rows of glyphs (144 x 288).

Each output glyph is 16 bytes (8 bits wide x 16 rows). The 9th column used at
render time is not stored; rasterization duplicates it for $C0-$DF as today.

Example:
  python3 utils/png_vga_font_to_8x16.py \\
    --png assets/img/IBM_VGA_8x16.png \\
    --ansi-out assets/roms/cards/secondsight/font_ansi_8x16.bin
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Pillow required: pip install Pillow", file=sys.stderr)
    sys.exit(1)

ATLAS_COL_STRIDE = 9
ATLAS_ROW_STRIDE = 17
GLYPH_W = 8
GLYPH_H = 16
FONT_BYTES = 256 * GLYPH_H  # 4096


def png_to_8x16(png_path: Path) -> bytes:
    im = Image.open(png_path).convert("RGBA")
    w, h = im.size
    expect_w = 16 * ATLAS_COL_STRIDE
    expect_h = 16 * ATLAS_ROW_STRIDE
    if w < expect_w or h < expect_h:
        raise SystemExit(f"{png_path}: expected at least {expect_w}x{expect_h}, got {w}x{h}")

    out = bytearray(FONT_BYTES)
    px = im.load()
    for g in range(256):
        sx = (g & 0x0F) * ATLAS_COL_STRIDE
        sy = (g >> 4) * ATLAS_ROW_STRIDE
        for gy in range(GLYPH_H):
            bits = 0
            for gx in range(GLYPH_W):
                r, gc, b, a = px[sx + gx, sy + gy]
                on = (a > 127) and ((r > 127) or (gc > 127) or (b > 127))
                if on:
                    bits |= 0x80 >> gx
            out[g * GLYPH_H + gy] = bits
    return bytes(out)


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--png",
        type=Path,
        default=Path("assets/img/IBM_VGA_8x16.png"),
        help="IBM VGA PNG atlas",
    )
    ap.add_argument(
        "--ansi-out",
        type=Path,
        default=Path("assets/roms/cards/secondsight/font_ansi_8x16.bin"),
        help="Output path for ANSI/IBM 8x16 bin",
    )
    args = ap.parse_args()

    args.ansi_out.parent.mkdir(parents=True, exist_ok=True)
    ansi = png_to_8x16(args.png)
    args.ansi_out.write_bytes(ansi)
    print(f"wrote {args.ansi_out} ({len(ansi)} bytes)")


if __name__ == "__main__":
    main()
