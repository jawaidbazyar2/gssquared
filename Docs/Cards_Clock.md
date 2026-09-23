# Clock cards

GSSquared offers two **slot** clocks for II / II+ / IIe machines, plus the built-in IIgs realtime clock (not a slot card).

## Thunderclock Plus (`thunder_clock`)

Emulates a Thunderclock Plus: ProDOS timestamps, **TIME SET**, and a host-synced counter (firmware ROM included). Time is taken from the host when the machine resets.

### Config editor

1. Open **+** or **Edit…**.
2. Click a slot (often **2** or **4**) and pick **Thunder Clock** / **Thunderclock**.
3. Save and launch.

### `.gs2`

```toml
[[cards]]
slot = 2
card = "thunder_clock"
```

Boot ProDOS and use utilities that read the Thunderclock, or `TIME SET` from the card’s firmware.

## Generic ProDOS clock (`prodos_clock`)

A simpler **read-only** ProDOS-compatible clock. Software can read the date/time; there is no TIME SET path.

```toml
[[cards]]
slot = 2
card = "prodos_clock"
```

## IIgs realtime clock

Every Apple IIgs platform has a motherboard RTC that follows the **host time zone**. Control Panel / NVRAM (battery RAM) is stored in the `.gs2` as `bram` when you close the machine — see [Writing Config Files Manually](ConfigFiles.md#machine-identity-id). You do not add a clock card for this.

## Related

- [Writing Config Files Manually](ConfigFiles.md)
- [Creating Custom System Configs](ConfigEditor.md)
