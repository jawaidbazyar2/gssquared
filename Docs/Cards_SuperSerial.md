# Super Serial Card

The Super Serial Card (SSC) is Apple’s classic slot serial card. In GSSquared it provides a virtual serial port you can attach to a **file** (capture) or a **modem** (TCP “dial-up”).

## What it does

- Emulates the Apple Super Serial Card (MOS 6551 ACIA + stock firmware ROM).
- Works in slots **1–7** on Apple II, II+, //e, and IIgs.
- Shares the same connection types as the IIgs built-in serial ports: **None**, **File**, **Clipboard**, or **Modem**.

Use it with ProTERM, Spectrum, ADTPro, printer-to-file capture, or any software that talks to an SSC.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Click a slot and choose **Super Serial**.
3. Under **Serial / Parallel**, click the port button for that slot and pick **Modem**, **File**, or **Clipboard** (see [Serial & Parallel Connections](SerialConnections.md)).
4. Save and launch.

### In a `.gs2` file

```toml
[[cards]]
slot = 2
card = "super_serial"

[[connections]]
slot = 2
device = "modem"
```

For file capture:

```toml
[[connections]]
slot = 2
device = "file"
path = "captures/ssc-output.bin"
```

If you omit `[[connections]]`, native builds default the SSC to **Modem**.

## Controlling the attachment while running

1. Press **F4** to open the Control Panel.
2. Click the serial-port button for the SSC slot.
3. Choose **None**, **File**, **Clipboard**, or **Modem**.

When a **File** attachment closes (idle timeout, reset, or you switch away from File), GSSquared shows a short on-screen toast with the capture filename.

## Using the virtual modem

With **Modem** attached, use Hayes-style commands from your terminal program. See [Serial / Modem](Serial_Modem.md) for `ATDT host:port` examples and hang-up.

## Related

- [Serial & Parallel Connections](SerialConnections.md)
- [Serial / Modem](Serial_Modem.md)
- [Writing Config Files Manually](ConfigFiles.md)
