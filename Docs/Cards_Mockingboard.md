# Mockingboard

The Mockingboard is a slot sound card (AY-3-8910 / 6522) used by many Apple II games and demos. GSSquared emulates one or two cards (up to four voices). The implementation passes the **mb-audit** test suite.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Click a slot (often **4**, and **5** for a second card) and pick **Mockingboard**.
3. Save and launch.

Two Mockingboards in different slots are allowed. Ultima V is the usual title that uses both.

### In a `.gs2` file

```toml
[[cards]]
slot = 4
card = "mockingboard"
```

## Using it

Boot software that expects a Mockingboard in that slot. Drive seek sounds are separate (motherboard / floppy); Mockingboard is the AY/6522 path.

**Settings → Mockingboard** (while a machine is running):

- **Mono Helper** decorrelates the right channel so Mockingboard stereo is easier to hear on a single speaker or a mixed-down recording.
- **Timbre** chooses the AY waveform. **Square** is the real chip. Soft Square, Warm Sine, Organ, and Sawtooth are host-side flavors for listening, not extra hardware.

A Mockingboard **C** also has an SSI-263 speech chip. GSSquared emulates it, so titles that talk through that chip can speak. No extra setting is required once the card is in a slot.

There is no Sound menu and no master-volume slider. Use the host OS volume.

## Related

- [Menus](Menus.md) — Mockingboard submenu (Mono Helper, Timbre)
- [Writing Config Files Manually](ConfigFiles.md)
