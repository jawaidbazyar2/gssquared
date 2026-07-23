# Intentionally Unimplemented

Features and hardware behaviors we have **chosen not to emulate** (or to leave as permanent no-ops), as distinct from open work in [Roadmap.md](Roadmap.md) / the todo list.

Add new entries here when we decide something is out of scope, obsolete, or unused enough that a stub would only add complexity.

---

## Apple IIgs — ROMBANK (`$C028` / STATEREG bit 1)

| | |
|-|-|
| **Hardware** | On ROM 00/01 boards, access to `$C028` toggles **ROMBANK**. The same bit appears as bit 1 of `$C068` (STATEREG). |
| **Effect** | When language-card ROM is mapped at `$D000–$FFFF`, the toggle flips **A14** into the system ROM for that window: normal `$FF/D000–$FFFF` vs alternate `$FF/9000–$BFFF` (shown in the `$D000` window). It does **not** swap banks `$FE`/`$FF`. |
| **ROM 01 firmware** | Unused. Apple docs say STATEREG ROMBANK “must always be 0.” IIgs firmware and GS/OS never touch it. |
| **ROM 03** | Removed from the hardware (CYA era). `$C028` / the STATEREG bit are inert. |
| **GSSquared** | `g_rombank` exists in the STATEREG bitfield for completeness / debugger display, but `$C028` is not handled and nothing remaps LC ROM from the bit. Behavior is “always 0,” which matches real software. |

**Related (ROM image layout, not ROMBANK):** Physical ROM01 dumps sometimes present banks `$FE`/`$FF` swapped relative to linear address space. Our ROM01 image is stored with `$FE` then `$FF` (bank `$FF` last) so the MMU can map the top `N` banks of the file linearly. ROM03’s four banks are already linear `FC–FF` that way. That normalization is independent of the ROMBANK softswitch.

---

## Cassette interface (`$C020` / `$C060`)

Listed in [Documentation.md](Documentation.md) as **no support planned**.

The cassette port existed on II / II+ / IIe only (not IIc / IIgs). Almost no modern use case; disk images cover the same software. Softswitches may exist as stubs or float depending on platform wiring — we are not building encode/decode or host WAV I/O for tape.

---

## II / II+ `REPT` key

The physical REPT key is not modeled. Host keyboards already supply autorepeat; mapping a separate REPT key would not improve compatibility for any known software we care about.

---

## When to put something here vs. a todo

| Put it here | Put it on the todo / roadmap |
|-------------|------------------------------|
| Firmware never uses it, or later hardware removed it | Needed for compatibility or a planned machine |
| Obsolete host/peripheral with no demand | Incomplete but wanted (e.g. printer, MIDI) |
| Emulation choice that permanently simplifies the model | Temporary stub until the real behavior lands |

If a “by choice” item later turns out to matter for a real title or diagnostic, promote it to a tracked issue and remove or revise the entry here.
