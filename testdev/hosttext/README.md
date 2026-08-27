# HOSTTEXT — Second Sight Host Text bounce demo

GS/OS S16 that talks to Second Sight in **slot 3**: `SetMode($03, $04)`
(80×25 Host Text), ANSI 8×16 font, then a planar 80×50 cell buffer in
hires page 1 (`$E0/2000` chars, `$E0/2FA0` attrs). Control block at
`$E0/3FE0`, IBM palette at `$E0/3FA0`.

Each VBL:

- increments `start_line` (hardware wrap scroll, frozen HUD rows)
- rotates the 16-color RAM palette
- bounces the hardware cursor on the status line
- every ~2 s page-flips to hires page 2 (`$4000` / `$4FA0`) — starfield vs
  credit-roll

Press any key to `SetMode` emulation and `rtl` back to GS/OS.

## Build

Merlin32 and CiderPress II (`cp2`) must be on disk:

```
make
```

Override paths if needed:

```
make MERLIN32=/path/to/Merlin32 CP2=/path/to/cp2
```

Produces `hosttext.po` (800K ProDOS) containing `HOSTTEXT` (type `$B3` S16).

## Run

1. Apple IIgs, **Second Sight** in slot 3, GS/OS booted from BazFast (or similar) on slot 7.
2. Mount `testdev/hosttext/hosttext.po` as another SmartPort volume beside GS/OS.
3. From the Finder, open that volume and launch **HOSTTEXT**.

Expect an 80×25 VGA-style screen: CP437 shade blocks, a scrolling message,
rainbow per-cell attributes, a status line that does not scroll, and a
periodic flip to a starfield page. The emulator `ss` debug panel should show
`hosttext` armed, `start=` climbing, and `buf=` toggling `$2000`/`$4000`.

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
image = "testdev/hosttext/hosttext.po"
```
