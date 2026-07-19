# Apple IIgs ROM 3

The **Apple IIgs (ROM 3)** machine loads its firmware from this directory.
The ROM images are Apple copyright and are not distributed here — supply
your own dumps:

| File | Size | What |
|------|------|------|
| `main.rom` | 262144 (256K) | Apple IIgs ROM 03 main ROM |
| `char.rom` | 16384 (16K)  | IIgs character generator (same as `../apple2gs/char.rom`) |

Drop both files here and select **Apple IIgs (ROM 3)** in the machine
picker. The IIgs MMU derives the bank `$FF` offset from the ROM size, so
the 256K image maps correctly (the stock `apple2gs` machine uses a 128K
ROM 01 image). ROM 03-specific hardware behavior is implemented
incrementally; this profile starts from the existing Apple IIgs device map.
