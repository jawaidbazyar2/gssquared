# Super Serial Card

Selectable slot card (`card = "super_serial"`) for Apple II / IIe / IIgs platforms (slots 1–7).

Emulates the Apple Super Serial Card: a MOS 6551 ACIA, two readable DIP switch blocks, the stock 2 KB firmware ROM, and slot IRQs. Virtual serial backends reuse the same `SerialDevice` / File / Modem modules as the IIgs SCC8530.

## Config

```toml
[[cards]]
slot = 2
card = "super_serial"
```

Default attachment (v1, same style as SCC hard-coding): **ModemDevice** on native builds, **FileDevice** under Emscripten. `[[connections]]` with `slot` is specified in [SystemConfigTOML.md](SystemConfigTOML.md) but is not yet applied at runtime.

Modem usage (Hayes AT commands over the attached device) is described in [Serial_Modem.md](Serial_Modem.md).

## I/O map

Addresses are `$C0nX` where `n = 8 + slot` (slot 2 → `$C0A0–$C0AF`):

| Offset | Register |
|--------|----------|
| `$C0n1` | DIPSW1 (read) |
| `$C0n2` | DIPSW2 (read; bit 0 = live CTS) |
| `$C0n8` | 6551 Data (TX write / RX read) |
| `$C0n9` | 6551 Status (read) / programmed Reset (write) |
| `$C0nA` | 6551 Command |
| `$C0nB` | 6551 Control |

## ROM layout

Firmware image: `roms/cards/ssc/341-0065-A.bin` (2048 bytes).

This image stores expansion ROM body at file offset `$000`, and the `$Cn00` page at offset `$700` (Apple listing / AppleWin-style packing):

- `$Cn00–$CnFF` ← file `$700–$7FF`
- `$C800–$CFFF` ← full file `$000–$7FF` (mapped when the slot’s `$Cnxx` space selects expansion ROM; `$CFFF` resets the map)

## DIP defaults (v1)

Hard-coded modem-friendly factory defaults (values **as read** by firmware; ON = 0, OFF = 1):

| Block | Value | Meaning |
|-------|-------|---------|
| DIPSW1 | `$EF` | 9600 baud, Communications mode |
| DIPSW2 | `$5A` | 8N1 modem format, auto-LF off; CTS asserted (bit0 = 0) |

DIP packing (from SSC firmware notes):

- DIPSW1: `S1 S2 S3 S4 Z Z S5 S6`
- DIPSW2: `S1 Z S2 Z S3 S4 S5 CTS`

SW2-6 (IRQ enable to the slot) is a hardware gate not present in the DIPSW2 read byte; v1 leaves it enabled.

## Architecture

Main emu thread: MMU handlers, 6551 registers, baud-timed TX/RX via `EventTimer`, slot IRQ via `InterruptController`.

Child thread: `FileDevice` / `ModemDevice` — chip ↔ device only through SPSC `SerialQueue` (`q_host` / `q_dev`).

DCD and DSR are held asserted so guest software does not stall waiting for carrier. Full RS-232 control messaging and config-driven `[[connections]]` are follow-ups shared with SCC work.
