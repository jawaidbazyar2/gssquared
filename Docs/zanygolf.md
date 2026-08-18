# Zany Golf (IIgs) — reverse-engineering / bug notes

Working notes from debugging the **black playfield** bug in GSSquared (Windmill hole and other levels). Music and HUD (Par / Strokes Left) work; the SHR playfield stays black.

Last updated: 2026-07-29.

## How to reproduce

```bash
./build/GSSquared -p 5 \
  -ds7d1=/Users/bazyar/src/IIgsDisks/wita2gs_0_81/wita2gs.pmap \
  --debug /tmp/gs2-zany.sock --no-quit-confirm
```

1. Wait ~5 seconds for the disk menu.
2. Type `zany` + Return (e.g. `clients/python/examples/type_to_emu.py`).
3. Enter a hole (Windmill). Playfield should be black; HUD + music OK.

Platform `-p 5` = ROM01. Same playfield failure also seen on ROM03 (`-p 6`).

## Symptom summary

| Item | Observed value |
|------|----------------|
| `NEWVIDEO` `$C029` | `$C1` — SHR on |
| `SHADOW` `$C035` | `$1E` / `$1F` — **SHR shadow inhibited** (bit 3 set) |
| `$E1/2000–$9FFF` | ~860 nonzero bytes (HUD only); mid-playfield empty |
| `$01/2000–$9FFF` | Often ~85% nonzero — **not** the displayed SHR buffer when shadow is off; mostly other RAM/code |
| Hot CPU loop | `$01/0D1B`–`$0DBF` mask blit |
| Music | Ensoniq `$C03D`/`$C03E` on IRQ — unrelated to the blit bug |

Display path: SHR pixels are scanned from Mega II **`$E1`**. With SHR shadow off, writes to bank `$01` do not update the screen. The game draws into an offscreen buffer and (presumably) copies to `$E1`; that pipeline is fed garbage because the **source** address is wrong.

---

## Two direct-page contexts

Zany reuses DP offsets for different meanings depending on `D`.

### A. LocInfo / setup — `D = $0C00`

Absolute addresses = `$0C00 + offset`. The QuickDraw-ish **LocInfo** block the blit reads lives at **`$0C28`**.

### B. Blit working DP — `D = $0E00`

Absolute addresses = `$0E00 + offset`. The mask-blit hot loop uses long pointers here (`[08]`, `($0C)` / `PLB` from `$0E`).

These can be live at the same time: LocInfo stays at `$0C28` while the blit runs with `D=$0E00`.

---

## LocInfo at `$0C28` (`D=$0C00` → DP `$28+`)

Layout as used by blit entry `$01/0C64` (16-bit words, little-endian):

| Abs | DP (D=`$0C00`) | Field | Role in blit |
|-----|----------------|-------|--------------|
| `$0C28` | `$28` | base low | Source base address low word |
| `$0C2A` | `$2A` | base high | Source base bank / high word (seen as `$0002` → bank `$02`) |
| `$0C2C` | `$2C` | width | Bytes per row (e.g. `$00F8`) |
| `$0C2E` | `$2E` | X / xoff | Horizontal offset (corrupted in bug: `$BD2F`) |
| `$0C30` | `$30` | **Y** | Treated as **16-bit Y** by blit; also abused as ptr low elsewhere |
| `$0C32` | `$32` | (Y high / pad) | Blit treats nearby words as scalars; other code stores **bank** here for long ptrs |
| `$0C34` | `$34` | **rows** | Row count for blit (corrupted: `$E938`) |
| `$0C36` | `$36` | | Related size / second dimension |
| `$0C38` | `$38` | destY / related | Further blit geometry (corrupted: `$3837`) |
| `$0C3A+` | `$3A+` | | Additional LocInfo / clip fields |

### Corrupted values on black Windmill hole (captured repeatedly)

```
base   = $02/1D0A
width  = $00F8
X      = $BD2F
Y      = $8531          ← should be a small scanline-ish value
rows   = $E938          ← should be a small row count
destY  = $3837
```

### Address math (why source becomes `$83/xxxx`)

Blit entry roughly does:

```text
product = width * Y          ; via $01/1360
src     = base + product + x_offsets
```

With the bad values:

```text
$00F8 * $8531 = $0081_0778
high word $81 + base bank $02 → bank $83
low + offsets → ~$8319
```

So **DB / source bank `$83`** is not a Memory Manager allocation in high RAM — it is **arithmetic overflow** from treating pointer-sized junk as Y.

### Sane values (after manually clearing `$0C2E`–`$0C39`)

When those bytes were zeroed, setup code rewrote:

| Site | Effect |
|------|--------|
| `$01/6312` | `$30 = $003C` (Y ≈ 60) |
| `$01/6941` | `$34 = $0013` (rows ≈ 19) |

Course graphics then appeared. So the blit engine itself is fine; **inputs** are wrong.

---

## Blit DP at `$0E00` (`D=$0E00`)

Captured on the bad hole while looping at `$01/0D1B`:

| Abs | DP | Bytes (LE) | Meaning |
|-----|-----|------------|---------|
| `$0E00` | `$00` | | Scratch / counters |
| `$0E02` | `$02` | | **Row / loop counter** — `DEC $02` near `$01/0DAF`; `BEQ` exit ~`$01/0DB1` |
| `$0E08`–`$0E0A` | `$08` | `ED C6 38` | **Dest long** `[08]` → **`$38/C6ED`** (offscreen buffer, valid RAM) |
| `$0E0C`–`$0E0E` | `$0C`/`$0E` | `19 83 83` | **Source**: addr `$8319`, bank byte `$83` → after `PLB`, **DB=`$83`** |
| `$0E1E`–`$0E20` | `$1E` | `00 20 E1` | **SHR screen pointer** `$E1/2000` (display base; not the blit dest) |

### Live bad-hole snapshot

| Pointer | Address | Mapped? | Sample |
|---------|---------|---------|--------|
| Source (`DB` + `$0C`) | `$83/8319` | **FLOAT** (ROM01 last RAM bank is `$7F`) | solid `$83` (`float_area_read` = bank#) |
| Dest `[08]` | `$38/C6ED` | RAM | filled with `$83` (copied from float src) |
| SHR | `$E1/2000` | Mega II | HUD only |

So: **source is float, dest is real, dest contents are float-colored garbage.** Screen stays black because nothing useful reaches `$E1` playfield.

---

## Code map (bank `$01`)

Addresses are `bank/offset` in program bank `$01` unless noted.

### Mask blit

| Address | Role |
|---------|------|
| `$01/0C64` | **Blit entry** — `PHP`/`PHB`/`REP #$30`, `TDC`, save D to `$0DCA`, copy LocInfo words from DP `$28+` into a working save area (`$0DCC+` style), set up multiply / source bank |
| `$01/0C80` | Alternate / related entry (callers searched; less hot than `$0C64` path) |
| `$01/0CFD` | **`STA $0E`** — store computed **source bank** into blit DP (after `ADC` of multiply high word onto base bank) |
| `$01/0D1B`–`$0DBF` | **Hot mask-blit loop** — `LDA ($0C),Y` / mask via `$22,X` / `STA [08],Y`; uses `DB` as source bank |
| `$01/0DAF` | `DEC $02` (row counter on blit DP) |
| `$01/0DB1` | `BEQ` → exit |
| `$01/0DB3` | `JMP` back into loop (`$0D1F` region) |

Loop shape (from live bytes at `$0D1B`): read source under current `DB`, combine with mask table via `$22,X`, write to long dest `[08]`, advance Y, `DEC $02`, loop.

### Multiply (width × Y)

| Address | Role |
|---------|------|
| `$01/1360` | **16×16→32 multiply**; high word returned in **X** (also poked into self-mod immediate) |
| `$01/1388` | Self-mod **immediate** holding multiply high word (seen as `$81` with bad Y) |

Caller path from blit entry: load width / Y from saved LocInfo, `JSR $1360`, then add high word to base bank and `STA $0E`.

### Writers that put **long pointers** into `$30` / `$32` (same abs as LocInfo Y)

These are **not** blit scalars; they stash 24-bit pointers at `$0C30`/`$0C32` when `D=$0C00`:

| Address | Behavior |
|---------|----------|
| `$01/187D` | `LDA #$17A4` / `STA $30`; `LDA #$0001` / `STA $32` → ptr **`$01/17A4`** |
| `$01/18A3` | Same pattern with **`$01/17B0`** |
| `$01/1C00` | ASCII tag **`MUSSHDRTRAK`** — music header path; uses `$30`/`$32` as **source long pointer**; copies `$2C`/`$2E` into `$34`/`$36` |
| `$01/15D8`, `$01/161E`, `$01/169F`, `$01/16D8`, `$01/1E2A`, … | Other `$30` updates (add/sub width, loads from tables) |
| `$01/3F04`, `$01/552F`, `$01/5DD0`, `$01/5EB5`, `$01/35FE`, … | Further `STA $30` sites seen on watchpoints |

**Structural bug hypothesis:** LocInfo Y/rows fields share DP slots with pointer-shaped data. If blit runs without those fields being rewritten as small integers, `width×Y` explodes into bank `$83`.

### Setup that writes **sane** Y/rows (after clear)

| Address | Role |
|---------|------|
| `$01/6312` | Writes Y `$003C` into LocInfo `$30` |
| `$01/6941` | Writes rows `$0013` into `$34` |

---

## Shadow register during gameplay

`$C035` watched with `BP_KIND_IO` write breakpoints during the hole main loop:

| Value | SHR shadow (bit 3) | Text1 inhibit (bit 0) |
|-------|--------------------|----------------------|
| `$1E` | **OFF** | clear |
| `$1F` | **OFF** | set |

Toggled rapidly by **ROM** routines:

- `$FF/BA93` — writes `$1F` (pattern `STA $C035`)
- `$FF/B7E5` — writes `$1E`

**SHR shadow never turns on** during the observed main loop. Only bit 0 chatters. Playfield is not expected to appear via `$01`→`$E1` shadowing; the game must blit/copy into `$E1` itself (HUD already does). Failure is upstream (bad source → useless offscreen buffer).

---

## Related memory

| Region | Notes |
|--------|------|
| `$38/C6ED` (grows in bank `$38`/`$39`) | Offscreen playfield buffer (dest `[08]`) |
| `$E1/2000` | SHR pixel buffer (scanner); HUD present, playfield empty when bug active |
| `$E1/9D00` | SCBs (often zero in captures) |
| `$E1/9E00` | Palette (nonzero when SHR active) |
| Bank `$83` | Unmapped on ROM01 (RAM through `$7F`); float reads return `$83` |
| Ensoniq `$C03D`/`$C03E` | Music IRQ path — works independently |

---

## What was ruled out / tried

### IIgs fast RAM size (partial red herring)

Installed size is **mobo + 8MB card**, hard-capped by the FPI’s 23-bit RAM decode at bank `$7F`:

| ROM | Total with 8MB card | Last RAM bank |
|-----|---------------------|---------------|
| ROM01 | 8MB (128KB card inaccessible) | `$7F` |
| ROM03 | 8MB (1MB card inaccessible) | `$7F` |

Implemented in `src/mmus/iigs_memory.hpp`. **Zany playfield still black** on ROM01 and ROM03 after the fix.

Bank `$83` remains float on both ROMs. LocInfo was still corrupted and `$E1` playfield still empty. So the bug is **not** “need bank `$83` to exist”; it is **bad Y/rows → computed bank `$83`**.

### Float-bus “fool the Memory Manager”

Early idea that float `$83` confused RAM probes was dropped: floating `$80+` is normal past installed RAM; KEGS does the same for unmapped banks.

---

## Debug recipe (GSSquared)

```bash
# Launch (see reproduce above), type zany, enter hole, then:

PYTHONPATH=clients/python/src python3 - <<'PY'
from gs2debug import Client, MEM_MAIN, BP_KIND_EXEC, BP_KIND_IO, BP_ACCESS_W, BP_FLAG_ENABLED
# Useful breakpoints:
#   EXEC $010C64  — blit entry (dump $0C28 LocInfo + D)
#   EXEC $010D1B  — hot loop (dump D=$0E00 longs, DB)
#   EXEC $011360  — multiply (A/X = factors / high word)
#   EXEC $010CFD  — STA $0E source bank
#   IO   $C035 W  — shadow (noisy; ROM toggles $1E/$1F)
PY
```

Trace capture: `GET_TRACE` → `/tmp/zany-golf-trace.bin`, decode with `./build/gstrace 65816 …`.

**Experiment that drew the course:** clear `$0C2E`–`$0C39`, let `$01/6312` / `$01/6941` refill Y/rows, continue.

---

## Open questions

1. **Who leaves pointer junk in `$0C30`/`$0C34` immediately before the playfield blit?** Music (`MUSSHDRTRAK`) and ptr writers at `$187D`/`$18A3` are suspects; need call-order / “last writer wins” relative to `$0C64`.
2. **Is LocInfo supposed to be copied from a level object each frame**, and that copy skipped/wrong under GSSquared timing?
3. **Buffer → `$E1` copy path** — not fully mapped; once source is sane, confirm how `$38/xxxx` reaches `$E1/2000`.
4. **Why KEGS works** — same binary presumably; difference may be timing, Memory Manager layout changing whether stale DP is overwritten, or an earlier init path we do not hit.

---

## Quick reference card

```
LocInfo @ $0C28 (D=$0C00):
  $28/$2A  base       $2C width    $2E X
  $30/$32  Y (blit) / long ptr (other)
  $34      rows       $38 destY…

Blit DP @ $0E00:
  $02      row counter
  $08      dest long  → $38/C6ED (offscreen)
  $0C/$0E  src addr/bank → bad: $83/8319
  $1E      SHR ptr    → $E1/2000

Code:
  $01/0C64 entry → $01/1360 mul → $01/0CFD STA bank
  $01/0D1B–0DBF  mask loop
  $01/1C00       MUSSHDRTRAK (ptr in $30)
  $01/187D/18A3  ptr stores to $30/$32

Video:
  NEWVIDEO=$C1  SHADOW=$1E/$1F (SHR shadow OFF)
```
