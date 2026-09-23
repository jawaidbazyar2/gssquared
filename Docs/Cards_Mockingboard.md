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

**Settings → Mono Helper** decorrelates the right channel so Mockingboard stereo is easier to hear on a single speaker or a mixed-down recording. See [Menus](Menus.md).

There is no Sound menu and no master-volume slider. Use the host OS volume.

## Related

- [Menus](Menus.md) — Mono Helper
- [Writing Config Files Manually](ConfigFiles.md)
