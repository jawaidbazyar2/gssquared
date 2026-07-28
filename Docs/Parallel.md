# Parallel Interface (for Printers)

The Apple Parallel Interface card had a ridiculously simple register interface, and firmware is available.

Selectable slot card (`card = "parallel"`) for Apple II / IIe / IIgs platforms (slots 1–7).

## Capture architecture (v1)

Parallel is output-only. Guest writes to `$C0n0` (`n = 8 + slot`) enqueue `MESSAGE_DATA` on the same SPSC `SerialQueue` path used by SSC and the IIgs SCC ports. A child-thread `FileDevice` drains the queue and writes capture files.

Default attachment: **FileDevice** always (native and Emscripten). No ModemDevice. A future ImageWriter backend can swap in via the same `SerialDevice` interface.

Capture filenames: `GS2.PARLn.YYYYMMDDHHMMSS` (same naming scheme as serial FileDevice).

Lifecycle:

- Open on first byte after close
- Idle-close after ~10s of no writes (FileDevice poll loop)
- Close on ctrl-reset (`MESSAGE_CLOSE`)
- Reopen on the next write
- On close: console log `file %filename closed`, and an OSD fade toast `File closed %filename`

Main emu thread never blocks on file I/O.

## Dot-Matrix Printers

libHaru is a C library (that interfaces well with C++) that can create PDF files.

So, explore the approach of implementing printer emulation threads, using libHaru to create PDF files.

The PDF file is the primary output. However, when printing, open a window and show recent portions of the image as it is generated. And it will also have a button to "close print job" which will close the PDF file and set up to create a new one.

PDF files are created somewhere - Desktop? Documents? Somewhere handy like that. With "GS2-Print-YYYYMMDD-HHMMSS.pdf" as the name.

This implies we will treat all output as simply a pixel map. Which is appropriate, as that is what dot matrix printers do. And, this will be easy to both display and stuff into a PDF.

No need to have cancel or anything like that since it's not real paper.

The Printer emulator should run as a separate thread, so it doesn't take up any time in the main loop. So the emu will chuck data to the printer thread.

This will also simplify the state machine management of the printer.

For testing, and initial deployment, we will want to have a printer capture mode that captures the raw data stream to the printer port and saves it to a file. Then have an initial standalone utility to convert those to PDF. That's the best way to get started. (v1 uses FileDevice capture as above.)

## Laser Printers

There was no software that handled PostScript on the IIe/IIc. People did use LaserWriters but those could apparently emulate an ImageWriter.
