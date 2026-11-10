# Printer Emulation

Spec for ImageWriter II printing. Not implemented. Byte capture is what ships today. Tracked as [#51](https://github.com/jawaidbazyar2/gssquared/issues/51).

Research notes that led here: [Imagewriter.md](Imagewriter.md), [EpsonMX80.md](EpsonMX80.md), [Parallel.md](Parallel.md). The user-facing capture card is [Cards_Parallel.md](Cards_Parallel.md).

## Summary

GSSquared keeps capturing printer bytes the way it does now. It does not rasterize them. A separate printer app, which the user downloads on their own, turns a byte stream into a page on screen and a PDF.

A port uses it when its `[[connections]]` device is `gs2print`. GS2 starts the app only when a print job actually begins. It writes a small `.gs2print` ticket and asks the operating system to open it. The ticket names a local socket that GS2 is already listening on. The printer app connects, its window appears immediately, and GS2 streams the guest's bytes as they arrive. Opened on its own, File → Open reads a raw printer dump and writes a PDF, with no emulator running.

The socket GS2 opens is a parent-chosen local name, not a fixed TCP port. macOS, Linux, and Windows all support that. The listen and connect calls live in a small platform library.

The same app can also listen, as a server the user starts. Another emulator, or a real Apple II with a serial-to-network adapter, then connects and sends the same raw bytes. That port is the user's choice and is shown in the window.

## What exists today

The parallel card (`card = "parallel"`, slots 1–7) is output-only. Guest writes to `$C0n0` enqueue `MESSAGE_DATA` on the same queue the Super Serial Card and the IIgs SCC ports use. A `FileDevice` thread drains the queue.

- Capture files are named `GS2.PARLn.YYYYMMDDHHMMSS`. Clipboard is the other attachment.
- The file opens on the first byte, closes after about 10 seconds of idle or on Ctrl-Reset (`MESSAGE_CLOSE`), and reopens on the next write.
- Closing shows an OSD toast with the filename.
- The main thread never blocks on file I/O.
- Config is `[[connections]]` with `device = "file"` or `device = "clipboard"`. Parallel allows `none`, `file`, and `clipboard`.

### Super Serial Card printer

`device = "printer"` is offered only on a Super Serial Card. It uses the same `FileDevice` as `device = "file"`: the dump is `GS2.SSCn.YYYYMMDDHHMMSS`, the file opens on the first byte, and it closes after about 10 seconds of idle or on Ctrl-Reset. Each guest byte is written to that dump and, on native builds, to a gs2print socket. On the first byte the printer worker binds a fresh local socket, writes a `.gs2print` ticket, and asks the OS to open the ticket. That open is spawned on the worker (`open` / `xdg-open` via `SDL_CreateProcess`, or `ShellExecuteEx` on Windows) and is not waited on, so the emulator thread keeps running and the worker keeps draining guest bytes into the dump. Bytes that arrive before the sidecar connects are sent when it does. If the open fails or nothing connects within about 5 seconds, an OSD toast says the printer app is not installed and the dump continues. The web build writes the dump only.

While Printer is attached, DIPSW1 mode bits are SIC P8 (SW1-5 ON, SW1-6 OFF). With the 9600 default that value is `$ED`. Stock SSC firmware then does not send ETX (`$83`) or wait for ACK (`$86`). Other attachments leave DIPSW1 at `$EF`, which is SIC P8A, so a File capture of `PR#` output can be only `$83`. Firmware latches the P8 auto-CR flag at `PR#` / card init, so reset after selecting Printer.

The sidecar app, PDF rendering, and offering this attachment on SCC or parallel are not implemented. SCC and parallel ports do not offer `printer`.

```toml
[[cards]]
slot = 1
card = "super_serial"

[[connections]]
slot = 1
device = "printer"
```

There is no ImageWriter, Epson, or PDF emulation in the emulator. The IIgs printer port (SCC channel A) uses the same device interface, so `gs2print` below covers it as well as the parallel card.

## Printer

Emulate the ImageWriter II. It was Apple's printer for this machine, and the Technical Reference Manual prints the character matrices. The Epson MX-80 / RX-80 ROM investigation ([EpsonMX80.md](EpsonMX80.md), `utils/mx80-rom-print.c`) is set aside. That ROM is an RX-80 dump, the glyphs are stored sideways with inverted bits and page-boundary padding, and the layout past the ASCII set was never settled.

Draw every page as a bitmap. Embedded PDF fonts would make smaller, selectable text, and ImageWriter-style fonts exist, but the printer has a user-defined character set, so a text font cannot represent every job. The PDF is a picture of the page.

The print head has 9 pins at 1/72 inch. Paper moves in 1/144 inch steps, which is how double-density output works. Graphics mode uses the top 8 pins, one byte per column. Buffer a page or more, because the paper can move up and down, not only forward.

Commands are ESC sequences: graphics, tabs, character set, and the two software-switch banks (`ESC Z a b` opens switches, `ESC D a b` closes them). An ESC followed by an unknown byte is ignored along with that byte. Colour (the four-band ribbon) can wait; the monochrome draft face is the first one to render.

### Fonts

Use the column tables from [mikedaley/web-a2e](https://github.com/mikedaley/web-a2e) `src/js/printer/`. They were transcribed from the ImageWriter II Technical Reference Manual through that project's font editor. They are not a dumped ROM. The repo is MIT.

`imagewriter-ii.js` loads three of them:

| File | Export | Face |
| --- | --- | --- |
| `imagewriter-ii-rom-draft.js` | `IW2_DRAFT_ROM` | Draft. 12 columns, last column always blank. Appendix C. |
| `imagewriter-ii-rom-nlq-fixed.js` | `IW2_NLQ_FIXED` | Near-letter-quality, fixed pitch. 16 columns, up to 18 bits. |
| `imagewriter-ii-rom-nlq-prop.js` | `IW2_NLQ_PROP_ROM` | NLQ proportional. Variable width; the trailing blank column is the spacer. |

Each glyph is an array of column values, left to right. Bit 0 is the top wire, bit 7 is wire 8, and bit 8 (values above `0xFF`) is wire 9, the descender. Codes cover `$20`–`$7E` and MouseText `$C0`–`$DF`. Locale tables (UK, IT, DE, SE, FR, ES, DK) replace `$23`, `$40`, `$5B`–`$5D`, `$60`, and `$7B`–`$7E`.

Draft is the face to implement first. `imagewriter-ii-rom-standard-fixed.js` and `imagewriter-ii-rom-standard-prop.js` are in the same directory and are not loaded by that class.

## Core

One library turns printer bytes into pages. The platform frontends only display the current page and own the window, the socket, and the menus. The library does not open a window or a socket.

Write it in C++. The printer is a stateful object: head position, switch banks, tabs, the custom character set, and the page under the head. That is the same language as GSSquared, and libHaru's C API calls from C++ directly.

Export a small C API for the frontends. Swift and AppKit on macOS, a Win32 window on Windows, and GTK on Linux can all call C. They should not depend on the C++ ABI. The C functions are: feed bytes, read the current page bitmap (pointer, width, height, stride), and finish the job.

The bitmap is one page, at the printer's dot pitch. A form feed, or the end of the job, commits that page and starts a fresh one. The frontend redraws from the current bitmap as bytes arrive, so the preview is the page in progress.

### PDF

libHaru writes the PDF. As each page is committed, the core adds it to the open libHaru document as one image page (`HPDF_LoadRawImageFromMem` of that bitmap). libHaru holds the document in memory and writes the file on save, so the PDF is written when the job ends, at `GS2-Print-YYYYMMDD-HHMMSS.pdf`. Pages are not deferred until the whole job has been parsed. The on-screen preview stays the bitmap; the frontend does not re-read the PDF to draw it.

## Connection

The attachment is a `[[connections]]` device named `gs2print`, next to `file` and `clipboard`. The user picks it in the config editor and in the Control Panel port button. File and clipboard capture stay as they are. That device name is not implemented. The SSC attachment `device = "printer"` (above) is what opens the ticket today, and it still writes the dump. There is no `device = "gs2print"` config value yet.

```toml
[[cards]]
slot = 1
card = "parallel"

[[connections]]
slot = 1
device = "gs2print"
```

Allowed on a parallel card and on a serial port used as a printer (IIgs SCC channel A, slot 1, and a Super Serial Card). It is not a modem and it is not offered in the web build. The platform default stays `file`; `gs2print` is selected, not assumed.

With `device = "gs2print"`, the first byte of a job runs the ticket and socket flow in this document. With `file` or `clipboard`, nothing about the sidecar runs.

## Sidecar

The printer app is a separate download. GSSquared does not ship it, spawn a copy beside its own binary, or link it in. The web build has no sidecar; it keeps file and clipboard capture.

The app has three jobs:

- A live GS2 job: connect to GS2, show the page as bytes arrive, write a PDF when the job ends.
- File → Open: read a raw printer dump and write a PDF, with no emulator running.
- A server: listen for a raw TCP connection from anything else that can open one.

The window is a small native UI on each platform: the page bitmap from the core, and a button that closes the job.

The sidecar registers itself as the handler for `.gs2print` when the user installs or first runs it. That is a UTI in its macOS bundle, an HKCU ProgID on Windows (same idea as [ProtocolHandlers.md](ProtocolHandlers.md) for `.gs2`), and a `.desktop` MIME type on Linux. A zip or AppImage only gains the association after that registration.

### When a job starts

Nothing is launched at emulator startup. The first byte after the port has been idle starts the job, which is the same moment `FileDevice` opens a capture file.

1. GS2 picks a fresh local socket name, binds, and listens.
2. GS2 writes a ticket (below) in the temp directory.
3. GS2 asks the operating system to open that file (`open` on macOS, `ShellExecute` on Windows, `xdg-open` on Linux). On the SSC Printer attachment this spawn runs on the printer worker and is not waited on. The worker keeps writing the capture file while the listen socket waits for a connection.
4. The association starts the printer app, or delivers the file to one that is already running. The app reads the ticket and connects.
5. GS2 accepts and writes the guest bytes as they arrive. The window is up before any dots are drawn.

GS2 listens, and the sidecar connects. The operating system starts the app, so GS2 has no pipe to it and cannot wait on its stdout. Binding before the open means the sidecar finds a listener when it starts.

Each job gets its own socket name, so two emulators, or two jobs in a row, do not share an endpoint. A second `.gs2print` opened while a window is already up is another job: macOS delivers it to the running app, and Windows and Linux normally start another process. Either way, one connection is one job.

### Socket

The name is a filesystem path GS2 invents, such as a file under the temp directory that includes the process id and a counter. There is no well-known TCP port.

The same idea works on macOS, Linux, and Windows. A platform library exposes listen, connect, read, write, and close, and hides the rest:

- macOS and Linux: an `AF_UNIX` `SOCK_STREAM` socket at that path.
- Windows: the same `AF_UNIX` socket (Winsock, Windows 10 version 1803 and later — the floor [gs2debug.md](gs2debug.md) already documents), or a `\\.\pipe\...` pipe with a generated name.

The path must fit in `sun_path`. Both ends must see the same path. Under MSYS2 that is a Windows path, because a `/tmp/...` path is translated by only one of the two processes.

The byte stream on the socket is the guest's printer bytes, one direction, GS2 to the sidecar. The ticket already carries the printer model, so the socket has no header. Closing the connection ends the job.

### `.gs2print` file

One extension, two contents. The first line is the magic `GS2PRINT 1`. The header is `key value` lines. A blank line ends the header.

A live ticket:

```
GS2PRINT 1
printer imagewriter-ii
socket /tmp/gs2-print-12345-1.sock

```

A saved dump uses the same header and then the raw byte stream:

```
GS2PRINT 1
printer imagewriter-ii

<raw printer bytes>
```

`printer` is `imagewriter-ii` for this spec.

### File → Open

File → Open takes a raw printer dump, the headerless byte stream a `file` capture writes (`GS2.PARLn.*` or any other file of printer bytes). The frontend feeds the whole file to the core, shows the pages, and the core writes the PDF beside the dump, with the same basename and a `.pdf` extension. A `.gs2print` saved dump (header, then bytes) opens the same way. No socket and no emulator are involved.

### When the job ends

The job ends when any of these happen:

- About 10 seconds pass with no bytes, matching the capture-file idle close.
- Ctrl-Reset sends `MESSAGE_CLOSE`.
- The user presses the sidecar's close-job button, which closes its end of the socket.
- GS2 closes its end.

The sidecar finishes the page, the core saves the libHaru document to `GS2-Print-YYYYMMDD-HHMMSS.pdf` somewhere the user can find it (Documents is the default), and the window stays open on the finished page. The next guest byte starts a new ticket and a new connection.

### Server mode

Server mode is for clients that were not launched by a `.gs2print` ticket and will never write one. The user starts the printer app and turns listening on. The window shows the address. Another emulator that can open a TCP connection, or a real Apple II whose serial adapter opens one, is pointed at that address.

The bytes on the connection are the printer bytes, one direction, with no header. One connection is one job. Closing it ends the job. About 10 seconds with no bytes also ends it, because a serial-to-network adapter often holds its TCP connection open across what the Apple II thinks are separate jobs. The next bytes start another page.

The port is stored in the app's settings and shown in the window. Port 9100 is the usual starting choice, because that is the ordinary "send raw bytes, then close" printer port. If it is already taken, the window says so and the user picks another. Nothing in GS2 or the sidecar assumes a compiled-in port for the ticket path above.

Listening is on localhost until the user opts in to the LAN. A real machine on the network is the reason to opt in. The adapter copies UART bytes onto the TCP connection and handles baud rate and the printer's busy line on the Apple II side. Software that only sends bytes works. Software that bit-bangs the printer's status lines, instead of the serial bridge, has no real ImageWriter answering those lines.

A GS2 ticket still works while the server is running. The app connects out to GS2's local socket for that job, and accepts inbound TCP for the others. The renderer sees one byte stream either way.

### When the sidecar is not installed

Opening the ticket fails, or nothing connects within a few seconds. GS2 shows an OSD toast saying the printer app is not installed, and keeps writing the raw capture file it already writes. The user can open that file in the sidecar later. Guest software is not stalled on the missing app.
