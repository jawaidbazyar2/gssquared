# Intentionally Unimplemented

Features and hardware behaviors we have **chosen not to emulate** (or to leave as permanent no-ops), as distinct from open work in [Roadmap.md](Roadmap.md) / the todo list.

Add new entries here when we decide something is out of scope, obsolete, or unused enough that a stub would only add complexity.

---

## Cassette interface (`$C020` / `$C060`)

Listed in [Documentation.md](Documentation.md) as **no support planned**.

The cassette port existed on II / II+ / IIe only (not IIc / IIgs). Almost no modern use case; disk images cover the same software. Softswitches may exist as stubs or float depending on platform wiring — we are not building encode/decode or host WAV I/O for tape.

---

## II / II+ `REPT` key

The physical REPT key is not modeled. Host keyboards already supply autorepeat; mapping a separate REPT key would not improve compatibility for any known software we care about.

---

## DMABank

The IIgs DMABank register is currently unimplemented.

---

## Diagnostic register

The IIgs diagnostic register (used to read back the contents of the character rom) is unimplemented.

---

## When to put something here vs. a todo

| Put it here | Put it on the todo / roadmap |
|-------------|------------------------------|
| Firmware never uses it, or later hardware removed it | Needed for compatibility or a planned machine |
| Obsolete host/peripheral with no demand | Incomplete but wanted (e.g. printer, MIDI) |
| Emulation choice that permanently simplifies the model | Temporary stub until the real behavior lands |

If a “by choice” item later turns out to matter for a real title or diagnostic, promote it to a tracked issue and remove or revise the entry here.
