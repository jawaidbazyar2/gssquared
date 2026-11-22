# Phantom Reads and Writes

Indexed stores on the 6502 family often take an extra bus cycle before the real write. That cycle is usually a **dummy (false, phantom) read**, not a write. The extra cycle still exists on 65C02; what changed is *whether that cycle touches the effective address*.

This matters on the Apple II because many I/O locations have **read side-effects** (6522 `IFR` timer flags, Disk II sequencer, language-card switches). A dummy read of the wrong register is a real bug.

Code: `src/cpus/base_6502.cpp` (`phantom_read`, `phantom_read_ign`, `phantom_write`, `write_direct_ind_x`, `address_abs_x_write`).

## Terminology

Sather and later write-ups mix these names:

| Term | Meaning |
|-|-|
| Dummy / false / phantom **read** | Extra bus **read** on a store or RMW. |
| Invalid access | Dummy cycle at the *un-carried* page: `(base_hi << 8) \| ((base_lo + index) & 0xFF)`. |
| Multiple access | Dummy cycle at the *final* effective address, then the write. |
| Phantom **write** | Extra bus **write** of the old value. NMOS RMW (`INC`/`DEC`/`ASL`/…) does this; 65C02 RMW dummy-**reads** instead. |

GSSquared: `phantom_read()` always hits the MMU. `phantom_read_ign()` still burns the cycle, but skips the MMU read unless `CPUTraits::full_phantom_reads` (Apple II cores leave that off — typically a re-read of PC that cannot affect `$Cxxx` I/O).

## What 65C02 changed (Sather UTAIIe 4-23 / 4-26–4-27)

NMOS had both **invalid** (wrong-page) accesses and **multiple** accesses. 65C02 dropped all invalid accesses and some multiple accesses. It kept enough multiple accesses that Disk II RWTS still works (`STA $C08F,X` must double-access the sequencer). 65816 (Apple IIgs) puts the NMOS-style dummy reads back.

| Instruction | Page cross | NMOS 6502 | 65C02 | 65816 |
|-|-|-|-|-|
| `STA abs,X` / `STA abs,Y` | no | dummy read of EA, then write | **same** (RWTS) | same as NMOS |
| `STA abs,X` / `STA abs,Y` | yes | dummy read of *wrong page*, then write | no invalid access; write EA | dummy read of wrong page (NMOS-like) |
| `STA (zp),Y` | either | dummy read of un-carried page, then write | **single write** (extra cycle is not an EA read) | dummy read like NMOS |
| `STA (zp)` (65C02-only) | n/a | — | single write, no dummy | same as 65C02 |
| RMW `abs,X` etc. | — | dummy **write** of old value | dummy **read** | emu-mode like NMOS; native dummy-read |

`STA abs,X` no-page-cross is the Disk II case: Sather 4-26, `STA $C08F,X` must hit `$C0EF` twice (slot 6). `STA $C0EF` (no index) will not.

`STA (zp),Y` becoming a single write is the Applesoft `POKE` fix. Sather 4-26 still says no-page-cross `STA (ZP),Y` can double-access `$C08F`; that sentence should have had his 65C02 asterisk. It is true of NMOS, not 65C02.

Hardware check (Nick Westgate, IIe + NCR 65C02A): language-card write enable needs two **reads** of `$C081`. `STA abs,X` supplies the extra read; `STA (zp),Y` does not, so LC RAM is not write-enabled. Thread: [Phantom reads on 6502, 65C02 and 65816](https://groups.google.com/g/comp.sys.apple2.programmer/c/qk7MZPRgVXI).

## `STA (zp),Y` in `write_direct_ind_x`

NMOS dummy-read address:

```
(base_hi << 8) | ((base_lo + index) & 0xFF)
```

i.e. `(base & 0xFF00) | (eaddr & 0xFF)` after `eaddr = base + index`. Do **not** add `index` a second time (`(eaddr + index) & 0xFF`); that hits the wrong 6522 register.

| CPU | Extra cycle | Bus |
|-|-|-|
| NMOS 6502 | yes | `phantom_read` of that address |
| 65C02 | yes (still 6 cycles) | `phantom_read_ign` — no I/O side effect |
| 65816 | yes | `phantom_read` (same as NMOS; `has_65816_ops` branch) |

Every `CPU65816_*` trait sets `has_65c02_ops = true` as well as `has_65816_ops`. `phantom_read_ign` is therefore `else if constexpr (has_65c02_ops)` **after** the 65816 branch, not `if (has_65c02_ops)` alone. Same `if / else if / else` order as the RMW helpers. IIe Enhanced is `CPU65C02Traits` (`has_65c02_ops`, not `has_65816_ops`).

## Tests that caught this

### mb-audit 11:14:02 — wrong dummy-read *address* (NMOS)

`STA (MBBase),Y` with `Y=$84` to chip-B `T1L_L`. Double-adding `index` dummy-read `$C408` (chip-A `T2C_L`) and cleared `IFR.T2` between underflow and the ISR.

Fix: `(base & 0xFF00) | (eaddr & 0xFF)` in `write_direct_ind_x` (and the page-cross dummy in `read_direct_ind_x`). See `src/devices/mockingboard/MB.md`.

### mb-audit 41:01:09 — dummy-read *must not happen* on 65C02

T263_1 / AppleWin GH#52 (Willy Byte). Arms T1, then stores to SSI-263 `SSI_FILFREQ` (`$Cn44`), which aliases VIA-A `T1C_L`. A dummy **read** of that address clears `IFR.T1`.

Addressing modes in the test: `STA abs,X`, `STA abs,Y`, `STA (zp),Y`.

- 65C02 `STA abs,X` / `STA abs,Y`: still dummy-read → T1 **cleared** (`Actual=00`).
- 65C02 `STA (zp),Y` (subtest `$09`): **no** dummy-read → T1 **still set** (`Expected=40`). Same for T2 / subtest `$0B`.
- NMOS and 65816: all three modes dummy-read. mb-audit: *“65816 behaves like 6502 for sta (zp),y”*.

AppleWin only synthesizes the 6522 false-read for `STA (zp),Y` (`$91`) when the CPU is NMOS, citing `UTAIIe:4-23`. `STA abs,Y` (`$99`) / `STA abs,X` (`$9D`) still false-read on 65C02 (`UTAIIe:4-27`).

If 41:01:09 reports `Expected: 40 Actual: 00` on IIe Enhanced, `STA (zp),Y` is still dummy-reading `T1C_L`.

## Indexed absolute stores

`address_abs_x_write()` always `phantom_read`s the un-carried page, then writes EA. That is correct for NMOS, 65C02 *no-page-cross*, and 65816. 65C02 *page-cross* should not invalid-access the wrong page (Sather 4-27); that split is not fully modeled here yet. mb-audit T263_1 does not page-cross `$Cn44`.

## Sources

- Jim Sather, *Understanding the Apple IIe*, pp. 4-23, 4-26–4-27 (`UTAIIe` in AppleWin comments).
- AppleWin [GH#52](https://github.com/AppleWin/AppleWin/issues/52) (Willy Byte) and `Mockingboard.cpp` false-read of 6522.
- mb-audit `chip-ssi263.a` `T263_1` / `@FalseRead`.
- [Phantom reads on 6502, 65C02 and 65816](https://groups.google.com/g/comp.sys.apple2.programmer/c/qk7MZPRgVXI) (Westgate, Dickey, Schmidt).
