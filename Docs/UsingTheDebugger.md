# Using the Debugger

GSSquared includes a built-in debugger for inspecting CPU execution, memory, video pages, and device state while the emulated Apple II is running. It is intended for diagnosing emulator behavior, reverse-engineering software, or stepping through code you are developing for the guest machine.

The debugger opens in a **separate window** from the main display (desktop builds only — it is not available in the web/Emscripten build).

For automated control from scripts or external tools, see [gs2debug — Python API for agents](gs2debug.md). Developer design notes live in [Debugger.md](Debugger.md); wire-format details are in [DebugProtocol.md](DebugProtocol.md).

---

## Opening and closing the debugger

| Method | Action |
| --- | --- |
| **F10** | Toggle the debugger window open or closed (works from the main emulator window or from the debugger window itself) |
| **OSD → Debug button** | Move the mouse over the main display to reveal the hover controls on the left; click **Debug** |

When you close the debugger window (F10 or the window close button) while the CPU is in single-step mode, execution resumes at full speed automatically.

---

## Window layout

The debugger window is resizable. Use the tab buttons at the top to show or hide panes; the window grows or shrinks horizontally as you open and close panes.

| Tab | Purpose |
| --- | --- |
| **Trace** | Instruction history, CPU registers, run-control buttons, and forward disassembly while stepping |
| **Monitor** | Text command line (Apple Monitor–style) with scrollable output |
| **Watch** | Live hex dumps of watched memory ranges plus optional device diagnostic panels |
| **Video** | Static thumbnails of guest video memory decoded as text, lo-res, hi-res, double hi-res, or SHR |

At least one pane is always visible. **Trace** is shown by default when you first open the debugger.

---

## Execution control

The debugger distinguishes **full-speed run** from **single-step mode**. Most run-control actions only apply while the CPU is in single-step mode (after you have stepped at least once, or after a breakpoint stops execution).

### Toolbar buttons (Trace pane)

These appear in the upper-right when the Trace pane is open:

| Button | Action |
| --- | --- |
| **>** | Step into — execute one instruction |
| **^** | Step over — if the current instruction is `JSR` / `JSL` / `JML`, run until control returns; otherwise same as step into |
| **>\|** | Step out — run until the next `RTS` / `RTL` |
| **>>** | Continue — resume full-speed execution |
| **Beam / Static** | Toggle main-window display while stepping (see below) |

### Main-window display while stepping (Beam vs Static)

When the CPU is in single-step mode, the **main emulator window** (not the debugger Video pane) has two display modes, selected with the **Beam** / **Static** button in the Trace toolbar:

| Mode | Button label | Main window behavior |
| --- | --- | --- |
| **Static** (default) | `Static` | After each step, advance the video scanner through a full frame and redraw — best for everyday program debugging |
| **Beam** | `Beam` | Show the **partial** cycle-accurate frame built up so far; draw a red **crosshair** at the emulated beam position (H/V) |

Use **Beam** when debugging cycle-timed graphics: demos that flip soft switches mid-scanline, floating-bus reads, or other code where you need to see *where* the emulated electron beam is relative to the picture. Use **Static** when you just want to see the current screen contents after each instruction.

The active mode also appears in the Trace status line (`Display: Beam` or `Display: Static`).

### Keyboard shortcuts (debugger window focused)

| Key | Action |
| --- | --- |
| **Space** | Step one instruction (enters single-step mode) |
| **Return** | Continue (full speed) |
| **O** | Step over |
| **R** | Step out |
| **T** | Toggle instruction tracing on/off |
| **B** | Toggle display of raw opcode bytes in the trace |
| **Up / Down** | Scroll the trace one line |
| **Page Up / Page Down** | Scroll the trace one page |
| **Home / End** | Jump to oldest / newest trace entries |
| **Alt+Left / Alt+Right** (macOS: **Cmd+Left / Cmd+Right**) | Jump to oldest / newest trace entries |
| **F10** | Close (or open) the debugger window |

Sound is muted while the CPU is in single-step mode.

---

## Trace pane

The Trace pane is the primary view for “what did the CPU just do?”

### Register bar

Below the status line, a compact register display shows the current CPU state:

- **6502 / 65C02** — `PC`, `A`, `X`, `Y`, `SP`, flags (`N V - B D I Z C`), emulation flag, and IRQ state.
- **65816** — `PB/PC`, `DB`, `DP`, `A`, `X`, `Y`, `SP`, flags (`N V M X B D I Z C`), emulation flag, and IRQ state.

The `M` and `X` flag columns reflect the current accumulator/index width on the 65816.

### Instruction log

Each traced instruction appears on one line with columns for cycle count, registers, program counter, opcode bytes (optional), mnemonic, effective address, access direction (`R`/`W`), data byte, and an optional symbol label.

- **Tracing** is **on by default**. Press **T** to disable recording new entries (useful if you only care about breakpoints and want less overhead).
- Press **B** to hide or show raw opcode bytes (`B)ytes: ON/OFF` in the status line).
- Scroll backward through up to 100,000 retained instructions.

When you are in **single-step mode**, up to ten lines of **forward disassembly** appear below the executed trace. The next instruction to run is highlighted. Forward disassembly assumes straight-line execution (branches are not taken).

### Breakpoint stops

When an enabled breakpoint fires (or a `BRK` instruction executes), the emulator switches to single-step mode and stops with the program counter on the stopping instruction. Data and I/O breakpoints stop immediately after the triggering access.

Breakpoints remain active even when the debugger window is closed, as long as they were set and not cleared.

---

## Monitor pane

The Monitor pane provides a command line at the bottom of the pane. Click the input area, type a command, and press **Enter**. Previous commands are recalled with **Up** and **Down** while the input field is focused.

Type **`help`** for a summary of commands. Addresses use **`BB/AAAA`** bank/offset form on the Apple IIgs (bank is sticky across commands). On 8-bit machines, or to continue working in the same bank, a bare hex address such as `0400` is enough.

### Inspecting and changing memory

| Command | Example | Description |
| --- | --- | --- |
| *address* | `C000` | Read one byte |
| *address*:*values* | `2000:AA 55 CC` | Write bytes |
| *range* | `2000.201F` | Hex dump (16 bytes per line with ASCII) |
| `set` | `set 2000 FF 00` | Write bytes (alternate syntax) |
| `move` | `move 1000.100F 2000` | Copy a range to a new address |

### Breakpoints

| Command | Example | Description |
| --- | --- | --- |
| `bp` | `bp C000.C0FF` | Set execution breakpoint on a range |
| `bp` | `bp` | List all breakpoints |
| `bpd` | `bpd C000 w` | Data breakpoint (read `r`, write `w`, or `rw`) |
| `bpi` | `bpi C010 rw` | I/O breakpoint ($C0xx region) - Matches I/O from any bank |
| `nobp` | `nobp 3` | Remove breakpoint by id |
| `nobp` | `nobp C000` | Remove execution breakpoint at address |

### Disassembly

| Command | Example | Description |
| --- | --- | --- |
| `list` / `l` | `l C000` | Disassemble from address |
| `l` | `l` | Continue disassembly from last position |
| `m` | `m 16` | Set assumed **M** flag width for disasm (8 or 16) |
| `x` | `x 8` | Set assumed **X** flag width for disasm (8 or 16) |

### Memory watches

| Command | Example | Description |
| --- | --- | --- |
| `watch` | `watch 40.4F` | Watch a memory range (opens the Watch pane) |
| `watch` | `watch` | List watches |
| `nowatch` | `nowatch 2` | Remove watch by id |

Watched regions refresh every frame in the Watch pane as hex + ASCII.

### Files and symbols

| Command | Example | Description |
| --- | --- | --- |
| `load` | `load "file.bin" 2000` | Load a binary file into memory |
| `save` | `save "out.bin" 2000.2FFF` | Save a memory range to a file |
| `sload` | `sload "labels.lbl"` | Load cc65/ca65 symbol labels |
| `slookup` | `slookup C000` | Look up a label for an address |
| `sclear` | `sclear` | Clear loaded symbols |

Label files use ca65 listing format (`al` lines), for example:

```
al C000 .Main
al C010 .Loop
```

Loaded labels appear in the Trace pane’s label column when addresses match.

### Memory map (IIgs / bank-switched systems)

| Command | Example | Description |
| --- | --- | --- |
| `map` | `map` | Show MMU read/write mapping for common pages |
| `map` | `map E0 E1` | Show mapping for a page range |

### Device diagnostic panels (Watch pane)

| Command | Example | Description |
| --- | --- | --- |
| `debug` | `debug "display"` | Enable a named diagnostic panel |
| `debug` | `debug` | List active panels |
| `nodebug` | `nodebug "keyboard"` | Disable a panel |

You can also toggle panels with the buttons at the top of the Watch pane. Available panels depend on the emulated platform and installed devices. Common names include:

| Panel | Typical platform | Shows |
| --- | --- | --- |
| `display` | All | Soft switches, video mode state |
| `iiememory` | Apple IIe | IIe memory configuration |
| `mmugs` | Apple IIgs | IIgs memory map |
| `keyboard` | All | Keyboard strobe / modifier state |
| `diskii` | With Disk II | Drive phase, track, motor |
| `mockingboard` | With Mockingboard | 6522 / AY register snapshot |
| `scc8530` | Apple IIgs | Serial chip registers |
| `es5503` | Apple IIgs | Ensoniq DOC state |
| `clock` | All | Emulator cycle counters / speed |

### Video thumbnails (Video pane)

| Command | Example | Description |
| --- | --- | --- |
| `video` | `video hgr1` | Add a preset thumbnail |
| `video` | `video hires E0/8000 ntsc` | Add with decode mode, address, render mode |
| `video` | `video` | List active views |
| `novideo` | `novideo 2` | Remove view by id |

**Preset names:** `text1`, `text2`, `80text1`, `80text2`, `gr1`, `gr2`, `hgr1`, `hgr2`, `dhgr1`, `dhgr2`, `shr`

**Decode modes:** `text40`, `text80`, `lores40`, `lores80`, `hires`, `hires_ns`, `dhgr`, `shr`

**Render modes:** `mono`, `ntsc`, `rgb` (ignored for SHR)

In the Video pane, each thumbnail has buttons to cycle decode and render mode, an address field (`BB/AAAA`, press Enter to apply), and **x** to remove the view. Thumbnails are static snapshots (not cycle-accurate) and use 1× horizontal by 2× vertical scaling to approximate Apple II pixel aspect ratio.

### Cycle-accurate display (main window)

While you debug, keep in mind that GSSquared gives you **two different ways to see video**:

| View | Where | How it works |
| --- | --- | --- |
| **Main emulator window** | Behind the debugger | Cycle-accurate **VideoScanner** — use **Beam / Static** (Trace toolbar) while stepping |
| **Debugger Video pane** | Thumbnails in the debugger | **Memory snapshot** — decodes guest RAM now, ignoring beam timing |

For everyday software debugging, use the **Video pane** or **Static** main-window mode. For cycle-timed graphics, switch the main window to **Beam** and step at **1 MHz** — a red crosshair marks the scanner position on the guest picture.

**Speed setting matters.** Use fixed clock rates (especially 1 MHz), not Ludicrous Speed, when debugging raster effects.

**Useful main-window keys:** **F2** (display engine), **F5** (scaling), **F4** (Control Panel / speed).

**Beam counters** — enable the Watch pane **`display`** panel (`debug "display"`) for H/V position, soft switches, and VBL/HBL flags.

---

## Typical workflows

### Stop on a known address

1. Open the debugger (**F10**).
2. Open the **Monitor** tab.
3. Enter `bp C000` (or your target address/range).
4. Press **Return** in the Trace pane (or click **>>** then step once) to run at full speed.
5. When the breakpoint hits, inspect registers and the trace, then **Space** to step.

### Watch a variable or buffer

1. `watch 0300.030F` in the Monitor.
2. Open the **Watch** tab to see live hex.
3. Run or step; the dump updates each frame.

### Inspect a hi-res screen page

1. `video hgr1` or click the **hgr1** preset in the **Video** tab.
2. Change decode/render with the per-view buttons if needed.
3. Adjust the base address if the program uses a non-standard page.

### Debug with cc65 symbols

1. Build your project with ca65 so it emits a `.lbl` file.
2. In the Monitor: `sload "MyProject.lbl"`.
3. Trace and disassembly lines show symbol names where addresses match.

---

## Trace file on exit

When you quit emulation (or exit the application while a machine is running), GSSquared automatically saves the instruction trace ring buffer to a binary file named **`gssquared-trace.bin`** in your documents folder. The file contains raw `system_trace_entry_t` records for offline analysis.

There is not yet a built-in menu command to save or reload a trace mid-session; use the automatic save on exit, or the external debug protocol for scripted capture.

---

## External debug protocol (optional)

Launch GSSquared with a Unix domain socket to allow external tools to pause, step, read memory, set breakpoints, inject keystrokes, and more — without using the debugger UI:

```bash
./build/GSSquared --debug /tmp/gs2.sock -p 3
```

See [gs2debug.md](gs2debug.md) for the Python client and examples. The UI debugger and the socket protocol can be used at the same time.

---

## Limitations and tips

- **Web build** — The debugger window is not supported in the browser build; use the desktop application for interactive debugging.
- **Performance** — Opening the debugger window or enabling breakpoints adds per-instruction checking during full-speed run. Tracing every instruction also has a cost; turn tracing off (**T**) if you only need breakpoints.
- **Step over / step out** — Only work while already in single-step mode (after **Space** or a breakpoint stop).
- **Forward disassembly** — Shown only in single-step mode; does not follow branches.
- **Video pane** — Thumbnails decode guest RAM in place (instant memory snapshot). For cycle-timed effects, use **Beam** mode on the main window; see [Beam vs Static](#main-window-display-while-stepping-beam-vs-static).
- **Audio** — Stepping mutes audio output for that frame; Mockingboard-heavy titles may behave differently with the debugger open.
