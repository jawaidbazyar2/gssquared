# Serial & Parallel Connections

GSSquared treats each serial or parallel port as a jack you can plug a virtual device into — the same idea as mounting a disk image on a drive.

## What you can attach

| Attachment | Serial ports (IIgs SCC, Super Serial) | Parallel card |
|------------|----------------------------------------|---------------|
| **None** | Port idle | Port idle |
| **File** | Capture TX data to a host file | Capture printer bytes to a host file |
| **Modem** | Virtual Hayes modem (TCP dial-out) | — |

When a **File** attachment closes, a short on-screen message shows the filename.

## Control Panel (while the machine is running)

1. Press **F4** (or click the Control Panel tab).
2. Find the **Serial / Parallel** buttons (between the slot list and the drive icons).
3. Click a port button.
4. Pick **None**, **File**, or **Modem** (Modem only where allowed).

On Apple IIgs machines you will see the built-in ports (often labeled like printer/modem). Slot cards such as Super Serial or Parallel appear as their own buttons.

## Config editor (saved with the machine)

When creating or editing a `.gs2` config:

1. Add the card (or use a IIgs platform for built-in serial).
2. Use the **Serial / Parallel** row in the editor — click a port, choose the attachment.
3. **Save**. Those choices become `[[connections]]` entries in the file.

## In a `.gs2` file

**IIgs built-in SCC** (no `slot`):

```toml
[[connections]]
port = "a"
device = "file"

[[connections]]
port = "b"
device = "modem"
```

**Slot card** (Super Serial or Parallel):

```toml
[[connections]]
slot = 2
device = "modem"
```

Optional `path` sets the capture filename for `device = "file"`.

If you omit `[[connections]]`, GSSquared applies sensible defaults (IIgs: file on one port and modem on the other on native builds; SSC → modem; parallel → file).

## Related

- [Super Serial Card](Cards_SuperSerial.md)
- [Parallel Interface](Cards_Parallel.md)
- [Serial / Modem](Serial_Modem.md)
- [Writing Config Files Manually](ConfigFiles.md)
