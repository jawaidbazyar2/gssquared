# Super Serial Card

The Super Serial Card (SSC) is Apple’s classic slot serial card. In GSSquared it provides a virtual serial port you can attach to a **file** (capture), a **printer** (the same capture, with the card in SIC P8 mode), the **clipboard**, a **modem** (TCP “dial-up”), or a **host serial** UART.

## What it does

- Emulates the Apple Super Serial Card (MOS 6551 ACIA + stock firmware ROM).
- Works in slots **1–7** on Apple II, II+, //e, and IIgs.
- Adds **Printer** on top of the IIgs built-in serial ports’ **None**, **File**, **Clipboard**, **Modem**, and listed **host serial port**. Printer is not offered on the built-in ports.

Use it with ProTERM, Spectrum, ADTPro, printer-to-file capture, or any software that talks to an SSC.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Click a slot and choose **Super Serial**.
3. Under **Serial / Parallel**, click the port button for that slot and pick **Modem**, **File**, **Printer**, **Clipboard**, or a host serial port (see [Serial & Parallel Connections](SerialConnections.md)).
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

For the printer test attachment (same dump as File, card set to SIC P8):

```toml
[[connections]]
slot = 2
device = "printer"
```

If you omit `[[connections]]`, native builds default the SSC to **Modem**. Reset after choosing **Printer** so the card firmware sees the new switches.

## Controlling the attachment while running

1. Press **F4** to open the Control Panel.
2. Click the serial-port button for the SSC slot.
3. Choose **None**, **File**, **Printer**, **Clipboard**, **Modem**, or a listed host serial port.

When a **File** attachment closes (idle timeout, reset, or you switch away from File), GSSquared shows a short on-screen toast with the capture filename. **Clipboard** copies the captured text to the host clipboard on close. Guest baud / line settings are passed through to a **host serial** port, along with CD / CTS / DSR so hang-up is visible to BBS software. Handshake rules: [SerialPortSpec.md](SerialPortSpec.md).

## Using the virtual modem

With **Modem** attached, use Hayes-style commands from your terminal program. See [Serial / Modem](Serial_Modem.md) for `ATDT host:port` examples and hang-up.

## Related

- [Serial & Parallel Connections](SerialConnections.md)
- [Serial / Modem](Serial_Modem.md)
- [Serial port spec](SerialPortSpec.md)
- [Writing Config Files Manually](ConfigFiles.md)
