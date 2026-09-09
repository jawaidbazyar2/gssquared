# ThunderClock Plus

Hardware and emulation spec for the Thunderware ThunderClock Plus, synthesized from:

- [web-a2e](https://github.com/mikedaley/web-a2e) (`src/core/cards/thunderclock/`) — HLE card: host-time snapshot, serial shift, real 2 KB firmware. No IRQ, no time-set.
- [a2fpga_core](https://github.com/BrentRector/a2fpga_core) (`hdl/thunderclock/`) — gate-level uPD1990AC plus card glue, including timer-pulse IRQ and C8 ownership.

Secondary checks: ThunderClock Plus manual, MAME `upd1990a.cpp` / `a2thunderclock.cpp`, existing GSSquared notes in this file’s previous revision.

This is the spec `src/devices/thunderclock_plus/` implements. See [GSSquared today](#gssquared-today).

## What the card is

A slot clock built around a NEC **uPD1990AC** serial calendar chip and a **2 KB** firmware EPROM (2716). One I/O location, 256-byte slot ROM, 2 KB expansion ROM.

It is the clock ProDOS finds by itself. At boot ProDOS scans slots 7→1 and treats a card as a ThunderClock if slot ROM bytes are:

| Offset | Value |
|--------|-------|
| `$Cn00` | `$08` |
| `$Cn02` | `$28` |
| `$Cn04` | `$58` |
| `$Cn06` | `$70` |

On match it sets `MACHID` (`$BF98`) bit 0, patches `$BF07–$BF08` to the firmware clock driver, and turns `$BF06` from `$60` (RTS) into `$4C` (JMP). Firmware entry points:

| Address | Role |
|---------|------|
| `$Cn08` | READ time (ProDOS / BASIC) |
| `$Cn0B` | WRITE time |

The chip has **no year**. ProDOS invents one from month + day-of-week (the well-known six-year table). Do not invent a year nibble in the 40-bit stream.

Typical software slots are **5** or **7**. Any slot 1–7 works. web-a2e prefers 5.

Not compatible with the No Slot Clock (DS1216). Different protocol, different software.

## Address map

`n` = slot. Device-select base is `$C080 + (slot << 4)` (`$C090` in slot 1, `$C0D0` in slot 5).

| Range | Function |
|-------|----------|
| `$C0n0`–`$C0nF` | Single control/status register, **aliased on all 16 offsets** |
| `$Cn00`–`$CnFF` | Slot ROM = firmware bytes `$000–$0FF` |
| `$C800`–`$CFFF` | Expansion ROM = firmware bytes `$000–$7FF` (same 2 KB chip) |

web-a2e names `$C0n8` as an “aux” location but treats it as the same register. The factory manual clears IRQ with `LDA $C088,Y` then `LDA $C080,Y`; that works if every offset aliases (a2fpga: **any** device-select read clears IRQ).

Firmware file is 2048 bytes. The high 1 KB is often `$FF`. Map the whole image at `$C800`; do not clip to 1 KB.

### C8 ownership

Standard Apple II expansion-ROM rules (see [C800–CFFF](C800-CFFF.md)):

1. Access to this card’s `$Cnxx` claims `$C800–$CFFF`.
2. Access to `$CFFF`, or to another slot’s `$Csxx`, releases the claim (when `INTCXROM` is off).
3. On the IIe, `INTCXROM` maps motherboard ROM over `$C100–$CFFF`; the card must not drive C8 then.

a2fpga implements that in the card. GSSquared already does it in `MMU_II` via `set_C8xx_handler`.

## Control register (`$C0nX`)

### Write → uPD1990AC pins

| Bit | Name | Chip pin | Meaning |
|-----|------|----------|---------|
| 0 | DI / DATA IN | DATA IN | Serial bit into the shift register (SHIFT mode) |
| 1 | CLK | CLK | Shift clock. **Rising** edge shifts when command is SHIFT |
| 2 | STB | STB | Strobe. **Rising** edge latches C2:C0 and runs the command |
| 3 | C0 | C0 | Command LSB |
| 4 | C1 | C1 | |
| 5 | C2 | C2 | Command MSB |
| 6 | IRQEN | *(card glue)* | Enable IRQ onto `/IRQ` (manual / Jace). **Not** a chip pin. a2fpga does not latch this bit |
| 7 | — | — | Unused on write |

a2fpga latches only `data[5:0]`. web-a2e looks at STB, CLK, and `data[5:3]`.

**Edge polarity (both sources, and the datasheet):**

- **STB 0→1**: latch command, execute it.
- **CLK 0→1**: one shift if the latched command is SHIFT.
- Holding a line high does not retrigger. Falling edges do nothing.

GSSquared uses **rising** edges.

### Read

| Bit | Meaning | web-a2e | a2fpga |
|-----|---------|---------|--------|
| 7 | DATA OUT (shift-register LSB, or 1 Hz square wave in REGISTER HOLD) | yes | yes |
| 6 | Live TP square wave (Thunderware `CLOCK`/`TUT` polls this) | 0 | 0 |
| 5 | IRQ asserted (latched timer pulse, not yet acked) | not implemented | yes |
| 4–0 | — | 0 | 0 |

Firmware reads DATA OUT with `LDA $C0n0` / `ASL` (bit 7 → carry).

## uPD1990AC commands (C2:C0)

Latched on STB rising. Encoded in write bits 5:3 (`cmd = (value >> 3) & 7`).

| C2:C0 | Value in bits 5:3 | Name | Action |
|-------|-------------------|------|--------|
| `000` | `$00` | REGISTER HOLD | Normal timekeeping. DATA OUT = 1 Hz square wave (a2fpga). Shift register idle |
| `001` | `$08` | SHIFT | CLK rising: `shift_reg ← {DI, shift_reg[39:1]}`. DATA OUT = `shift_reg[0]` |
| `010` | `$10` | TIME SET | `time_counter ← shift_reg` |
| `011` | `$18` | TIME READ | `shift_reg ← time_counter`. DATA OUT immediately shows the new LSB. Does **not** require a CLK first |
| `100` | `$20` | TP 64 Hz | Timer pulse 64 Hz |
| `101` | `$28` | TP 256 Hz | Timer pulse 256 Hz |
| `110` | `$30` | TP 2048 Hz | Timer pulse 2048 Hz |
| `111` | `$38` | TP 4096 Hz / TEST | a2fpga: 4096 Hz pulse. web-a2e comments this as chip test mode. Manual only documents 64 / 256 / 2048 |

Firmware/manual interrupt-rate “control characters” are these TP commands, issued through the same STB path.

web-a2e implements TIME READ and SHIFT well enough for ProDOS. It documents the other codes and ignores them.

## 40-bit time format

LSB of the shift register is the first bit the 6502 sees (DATA OUT, bit 7). Within each nibble, bits are **LSB first**.

| Bits | Field | Encoding |
|------|-------|----------|
| 0–3 | Second ones | BCD 0–9 |
| 4–7 | Second tens | BCD 0–5 |
| 8–11 | Minute ones | BCD 0–9 |
| 12–15 | Minute tens | BCD 0–5 |
| 16–19 | Hour ones | BCD 0–9 |
| 20–23 | Hour tens | BCD 0–2 |
| 24–27 | Date ones | BCD 0–9 |
| 28–31 | Date tens | BCD 0–3 |
| 32–35 | Day of week | 0–6, Sunday = 0 |
| 36–39 | Month | **binary** 1–12, not BCD |

a2fpga `time_counter` packing (bit 0 = seconds ones):

```
[39:36] month | [35:32] dow | [31:28] date tens | [27:24] date ones
[23:20] hour tens | [19:16] hour ones | [15:12] min tens | [11:8] min ones
[7:4] sec tens | [3:0] sec ones
```

SHIFT right by one: new DI becomes bit 39; old bit 0 is discarded after being read.

web-a2e’s `docs/thunderclock-debug.md` lists month first. That note is stale. The C++ and unit tests match the table above (seconds first). Use the tests, not that debug page.

Month has no year and no leap-day. a2fpga uses Feb = 28 always.

## Read-time sequence

What the firmware (and web-a2e tests) do:

1. Drive STB low (and usually CLK low) so the next STB is a rising edge.
2. Write `$18 | STB` (`$1C`): TIME READ, STB rising. Chip copies `time_counter` → `shift_reg`. Bit 0 is already on DATA OUT.
3. Read `$C0n0`; bit 7 is bit 0 of the stream.
4. For bits 1–39: drop CLK, raise CLK, read bit 7. Optionally keep TIME READ + STB asserted; command is already latched.
5. Some firmware then issues SHIFT (`$08`) and continues clocking. After TIME READ, a2fpga only shifts while the **latched** command is SHIFT. web-a2e also advances the bit index if the latched command is still TIME READ.

Compatible behavior:

- After TIME READ, present `shift_reg[0]` immediately.
- Each CLK rising in SHIFT moves one bit.
- Also advancing on CLK while still in TIME READ (web-a2e) matches firmware that never issues SHIFT. Prefer a2fpga: TIME READ loads; SHIFT clocks. If a driver only strobes `$18` and then toggles CLK with `$18` still in the write data, STB stays high so the command is **not** re-latched; the latched command remains TIME READ. web-a2e still shifts. a2fpga would not.

**Emulator choice:** accept CLK rising as a shift whenever the last latched command is TIME READ **or** SHIFT. That covers both firmwares and both sources.

## Set-time sequence

a2fpga only (web-a2e has no TIME SET):

1. SHIFT mode. For 40 CLK rising edges, present each data bit on DI (bit 0). First bit written is seconds-ones LSB (same order as read).
2. STB with TIME SET (`$10 | STB` = `$14`). `time_counter ← shift_reg`.

The chip then keeps time from that value.

## Timekeeping models

| | web-a2e | a2fpga |
|--|---------|--------|
| Source of time | `localtime()` on every TIME READ | Internal BCD counter, +1 s from the FPGA clock |
| TIME SET | ignored | loads the counter |
| Apple Reset | N/A (stateless snapshot) | Chip **does not** reset (battery). Card IRQ latch **does** |
| Power-on | host now | 00:00:00, Sunday 1 January |

For GSSquared, either model is valid:

- **HLE (web-a2e):** TIME READ snapshots the host clock. Enough for ProDOS timestamps. TIME SET can be a no-op or can store an offset.
- **Chip (a2fpga):** keep a 40-bit BCD counter, tick at 1 Hz (or derive from CPU cycles). Needed if Thunderware `TIME` / set-clock utilities must stick, and for IRQ-driven apps that assume the chip is free-running.

Recommended: HLE snapshot for TIME READ, plus store TIME SET into the same 40-bit register so a following TIME READ returns what was written until the next host snapshot policy. Simplest correct policy: after TIME SET, use the loaded value and advance it with a 1 Hz tick (do not jump back to host time until the next cold start).

## Interrupts

web-a2e: not implemented.

a2fpga:

1. A TP command (`$20` / `$28` / `$30` / `$38`) makes `tp_out` a square wave at that rate.
2. **Rising** edge of `tp_out` sets `irq_status`.
3. `/IRQ` is asserted while `irq_status` is set (and the card is enabled).
4. **Any read** of `$C0nX` clears `irq_status`.
5. Read bit 5 reflects `irq_status` **before** the clear (same cycle: present the bit, then clear).

Factory manual (card glue, not in a2fpga):

1. Put the ISR at `$03FE/$03FF`.
2. Write `$40` (IRQEN) to the control register, then `CLI`.
3. Rates 64 / 256 / 2048 Hz via the TP commands (SET/PROTECT switch on the real card; ignore the switch in software).
4. ISR ack: `LDA $C088,Y` then `LDA $C080,Y` (both alias to the same read-clear).

`$40` is bit 6 only; C2:C0 = HOLD. So IRQ enable is **not** a uPD1990 command. a2fpga never looks at bit 6: IRQ follows TP mode alone.

**Target for GSSquared:**

- Latch IRQEN from write bit 6.
- Latch TP rate from C2:C0 when STB rises on a TP command. Default 64 Hz at power-on; do **not** cancel TP on HOLD / SHIFT / TIME READ (Thunderware TEST only writes `$40`). TIME SET forces 64 Hz.
- On each TP rising edge, if IRQEN, set IRQ asserted and pull `/IRQ`.
- Read `$C0nX`: DATA OUT in bit 7, live TP in bit 6, IRQ asserted in bit 5; then clear asserted.
- Do not require a dedicated `$C0n8` decoder; aliasing is enough for the manual’s two-read ack.

ISR software must not call `$Cn08` firmware (that claims `$C800` and can evict another card). Direct `$C0n0` bit-bang only. That is a software rule, not extra hardware.

## Reset and persistence

| Event | Chip time | Shift / command | IRQ latch | IRQEN |
|-------|-----------|-----------------|-----------|-------|
| Apple Reset (Ctrl-Reset, PR#) | keep (battery) | a2fpga keeps chip state | clear | a2fpga clears `control_reg` |
| Emulator power-on / cold start | host now, or midnight 1 Jan | HOLD, empty shift | clear | 0 |

a2fpga splits `device_reset_n` (chip) from `system_reset_n` (card). Match that: **do not zero the time of day on warm reset.**

## Firmware ROM

Ship the real image (`roms/cards/tcp/tcp.rom` in this tree; MAME CRC `1b99c4e3`).

```
https://mirrors.apple2.org.za/Apple%20II%20Documentation%20Project/Interface%20Cards/Clock/Thunderware%20Thunderclock/ROM%20Images/Thunderclock%20Plus%20ROM.bin
```

Do not synthesize a fake `$Cn00` stub if this ROM is present. ProDOS and Thunderware utilities execute it.

## Source disagreements (resolved)

| Topic | web-a2e | a2fpga | Spec |
|-------|---------|--------|------|
| STB / CLK edge | rising | rising | **rising** |
| After TIME READ, first bit | immediate, no CLK | immediate (`shift_reg[0]`) | **immediate** |
| CLK while command still TIME READ | advances bit | no shift (need SHIFT) | **accept both** (TIME READ or SHIFT) |
| I/O aliases | all 16 | all DEVSEL | **all 16** |
| DATA OUT bit | 7 | 7 | **7** |
| IRQ status bit | — | 5 | **5** |
| IRQ enable bit 6 | — | not implemented | **implement** (manual) |
| IRQ clear | — | any `$C0nX` read | **any read** |
| TIME SET | no | yes | **yes** if aiming at utilities |
| Time source | host snapshot | BCD + 1 Hz | see [Timekeeping](#timekeeping-models) |
| Command `111` | test mode | 4096 Hz | either; firmware uses 64/256/2048 |
| Debug-doc nibble order | wrong (month first) | seconds first | **seconds first** |

## GSSquared today

`src/devices/thunderclock_plus/thunderclockplus.cpp`:

- Loads `tcp.rom`; maps `$Cnxx` and `$C800` via `map_c1cf_page_read_only`.
- All 16 `$C0nX` aliases; **rising** STB/CLK.
- TIME READ / TIME SET / SHIFT / HOLD / TP; 40-bit stream is seconds-ones first.
- Host snapshot on TIME READ until TIME SET; then 1 Hz BCD tick (Feb = 28).
- Power-on default TP 64 Hz; HOLD / SHIFT / TIME READ / warm reset leave TP running; TIME SET forces 64 Hz.
- Read: DATA OUT bit 7, live TP square wave bit 6, IRQ status bit 5 (then clear).
- IRQEN (write bit 6); `/IRQ` while `irq_status && irqen`.
- Warm reset keeps time; cold start snapshots the host.

## Implementation checklist

1. Switch STB/CLK to **rising** edges; keep previous pin state.
2. Latch `cmd = (value >> 3) & 7` on STB rising; run TIME READ / TIME SET / TP / HOLD / SHIFT.
3. Present `shift_reg[0]` on bit 7 immediately after TIME READ; shift on CLK rising in SHIFT (and TIME READ, for web-a2e firmware).
4. Keep the 40-bit layout in the table above (seconds ones first, month last, DOW 0 = Sunday).
5. Alias `$C0n0–$C0nF`.
6. IRQEN (bit 6), TP rates, latch on TP rising, status on read bit 5, clear on read, raise `/IRQ`.
7. TIME SET copies shift → time; warm reset does not wipe the clock.
8. Drop per-access `fprintf`; keep a `DEBUG_THUNDERCLOCK` trace if needed.

## References

- web-a2e: `src/core/cards/thunderclock/thunderclock_card.{hpp,cpp}`, `tests/unit/test_thunderclock.cpp`
- a2fpga: `hdl/thunderclock/thunderclock_card.sv`, `hdl/thunderclock/upd1990.sv`, [README](https://github.com/BrentRector/a2fpga_core)
- [ThunderClock Plus manual (PDF)](https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/clocks/ThunderClock%20Plus.pdf) — firmware entry points, IRQ ack, 64/256/2048 Hz
- MAME: `src/devices/machine/upd1990a.cpp` (chip), `src/devices/bus/a2bus/a2thunderclock.cpp` (card; DATA OUT only, no IRQ wire)
- [C800–CFFF mapping](C800-CFFF.md)
