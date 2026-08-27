# GPUTEST — Second Sight GPU bounce demo

GS/OS S16 that talks to Second Sight in **slot 3**: `SetMode($5C, $03)` (640×480 GPU), uploads three 128×128 RGB555 textures, then each frame sends a small CSB (`Clear` / three `DrawTexture` / `Present` VBL / `End`). The 65816 never blits 640×480.

Press any key to free the textures, return to emulation mode, and `rtl` back to GS/OS.

This stage assumes the emulator has GPU mode. Do not wait on `GetCapabilities`.

## Build

Merlin32 and CiderPress II (`cp2`) must be on disk:

```
make
```

Override paths if needed:

```
make MERLIN32=/path/to/Merlin32 CP2=/path/to/cp2
```

Produces `gputest.po` (800K ProDOS) containing `GPUTEST` (type `$B3` S16).

## Run

1. Apple IIgs, **Second Sight** in slot 3, GS/OS booted from BazFast (or similar) on slot 7.
2. Mount `testdev/gputest/gputest.po` as another SmartPort volume beside GS/OS.
3. From the Finder, open that volume and launch **GPUTEST**.

Expect three 128×128 patterns (solid red, green/blue checker, RGB555 gradient) bouncing at display rate. The emulator `ss` debug panel should show GPU mode and three texture handles.

Optional config shape (see [SystemConfigTOML.md](../../Docs/SystemConfigTOML.md)):

```toml
[[cards]]
slot = 3
card = "second_sight"

[[cards]]
slot = 7
card = "bazfast3"

[[storage]]
slot = 7
drive = 1
image = "volumes/GSOS.po"

[[storage]]
slot = 7
drive = 2
image = "testdev/gputest/gputest.po"
```
