# Serial & Parallel Connections

GSSquared treats each serial or parallel port as a jack you can plug a virtual device into — the same idea as mounting a disk image on a drive.

## What you can attach

| Attachment | Serial ports (IIgs SCC, Super Serial) | Parallel card |
|------------|----------------------------------------|---------------|
| **None** | Port idle | Port idle |
| **File** | Capture TX data to a host file | Capture printer bytes to a host file |
| **Clipboard** | Capture TX to the host clipboard | Capture printer bytes to the host clipboard |
| **Modem** | Virtual Hayes modem (TCP dial-out) | — |
| **Serial** | Real host UART (macOS `/dev/cu.*`, skipping Bluetooth/debug/wlan system callouts; Windows `COMn`, skipping Bluetooth modem mappings; Linux `/dev/ttyUSB*` `/dev/ttyACM*` `/dev/ttyAMA*`, skipping Bluetooth/debug) | — |

When a **File** attachment closes, a short on-screen message shows the filename. When a **Clipboard** attachment closes (idle ~10s, or Ctrl-Reset on parallel), high-bit-stripped text is copied to the host clipboard and a toast reports the byte count. Newlines are normalized to LF; IIgs “Add LF after CR” CRLF pairs become a single line break.

## Control Panel (while the machine is running)

1. Press **F4** (or click the Control Panel tab).
2. Find the **Serial / Parallel** buttons (between the slot list and the drive icons).
3. Click a port button.
4. Pick **None**, **File**, **Clipboard**, **Modem**, or a host serial port listed by name (Modem and host ports only on serial jacks). The host-port list is rescanned every 2 seconds while the control panel or config editor is open, so plugging or unplugging a USB serial dongle updates the picker.

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

Optional `path` sets the capture filename for `device = "file"`. For `device = "serial"`, `path` is the host port name stored as written (`/dev/cu.usbserial-…`, `cu.usbserial-…`, `COM3`, `/dev/ttyUSB0`, …) — it is not resolved relative to the config file.

```toml
[[connections]]
slot = 2
device = "serial"
path = "cu.usbserial-A50285BI"   # macOS; on Windows use path = "COM3"; on Linux, /dev/ttyUSB0
```

Guest software programs baud / data / parity / stop bits on the SCC or 6551; those settings are passed through to the host port. If the dongle is unplugged, the emulator keeps the attachment and retries. On Linux, `/dev/ttyUSB*` is owned by group `dialout` (some distros use `uucp`); the local user must be in that group and re-login before GSSquared can open the port.

If you omit `[[connections]]`, GSSquared applies sensible defaults (IIgs: file on one port and modem on the other on native builds; SSC → modem; parallel → file).

## Related

- [Super Serial Card](Cards_SuperSerial.md)
- [Parallel Interface](Cards_Parallel.md)
- [Serial / Modem](Serial_Modem.md)
- [Writing Config Files Manually](ConfigFiles.md)
