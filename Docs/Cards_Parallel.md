# Parallel Interface

The Parallel Interface card sends printer (or other Centronics-style) output from Apple II software to a file on your computer.

## What it does

- Emulates an Apple Parallel Interface card in slots **1–7**.
- Captures guest output to a host file (one byte stream per “job”).
- Shows a short on-screen toast with the filename when a capture file closes.

ImageWriter / PDF printer emulation is not required for basic capture — any software that prints to a parallel card will write to the attached file.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Click a slot and choose **Parallel**.
3. Under **Serial / Parallel**, click that port and choose **File** (parallel ports only allow **None** or **File**).
4. Save and launch.

### In a `.gs2` file

```toml
[[cards]]
slot = 1
card = "parallel"

[[connections]]
slot = 1
device = "file"
```

You can set an explicit path:

```toml
[[connections]]
slot = 1
device = "file"
path = "printouts/session.bin"
```

If you omit `[[connections]]`, parallel defaults to **File**.

> Older configs used `output = "..."` on the card entry. Prefer `[[connections]]` instead; legacy `output` is ignored at runtime.

## Controlling capture while running

1. Press **F4** for the Control Panel.
2. Click the parallel-port button.
3. Choose **File** to capture, or **None** to disconnect.

Capture files use names like `GS2.PARLn.YYYYMMDDHHMMSS`. A file opens on the first byte written, closes after ~10 seconds of idle output or on Ctrl-Reset, and reopens on the next write. Closing shows a toast with the filename.

## Related

- [Serial & Parallel Connections](SerialConnections.md)
- [Writing Config Files Manually](ConfigFiles.md)
