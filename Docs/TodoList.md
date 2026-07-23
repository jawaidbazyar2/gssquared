# GSSquared Todo List

Curated from open items, unchecked boxes, status tables, and known issues across `Docs/*.md`, plus open GitHub issues.

Many older checkboxes in `DevelopLog.md` are historical; this list prefers items that still appear open in feature docs, compatibility notes, or the roadmap. When a doc is clearly stale relative to `Roadmap.md`, the roadmap wins and the stale item is omitted or called out.

GitHub issue list last synced: 2026-07-19. Re-check with `gh issue list --state open`.

---

## Roadmap (release targets)

From [Roadmap.md](Roadmap.md):

### Release 0.10
- [ ] Provide flexible AI-drivable binary protocol debugger interface

### Release 1.0
- [ ] Fix all known / pending bugs

### Post-1.0
- [ ] Refactor Mockingboard to use new fixed-point synth
- [ ] Optimize / cache UI elements

---

## Open GitHub issues

From [jawaidbazyar2/gssquared](https://github.com/jawaidbazyar2/gssquared/issues) (`gh issue list --state open`):

### Bugs
- [ ] [#133](https://github.com/jawaidbazyar2/gssquared/issues/133) Unclaimed Sound Interrupt — QFTO (standard Sound Tools) dies to blue-screen after intro sound starts
- [ ] [#125](https://github.com/jawaidbazyar2/gssquared/issues/125) 5.25" drive missing when returning to Finder from GS/OS app; returning via P8 can leave continuous polling
- [X] [#124](https://github.com/jawaidbazyar2/gssquared/issues/124) Floating bus / edge artifacts on Sather Little Text Window demo
- [ ] [#117](https://github.com/jawaidbazyar2/gssquared/issues/117) Locksmith 6.0 fails to copy Bilestoad (half/quarter-track read)
- [ ] [#112](https://github.com/jawaidbazyar2/gssquared/issues/112) `$C021` (IIgs): determine correct RESET state (mono bit not cleared by RESET)
- [ ] [#101](https://github.com/jawaidbazyar2/gssquared/issues/101) textfunk timing incorrect — white border starts too early (memory access slowdown missing?)

### Enhancements / features
- [ ] [#140](https://github.com/jawaidbazyar2/gssquared/issues/140) International keyboards on GS (e.g. AZERTY)
- [ ] [#81](https://github.com/jawaidbazyar2/gssquared/issues/81) Make OA / CA key assignments configurable
- [ ] [#77](https://github.com/jawaidbazyar2/gssquared/issues/77) Debugger: render specified display page (`hgr1`, `80text1`, …) into watch pane
- [ ] [#73](https://github.com/jawaidbazyar2/gssquared/issues/73) Disasm: share trace disassembly; M/X width commands; track REP/SEP
- [X] [#69](https://github.com/jawaidbazyar2/gssquared/issues/69) Debugger UI: play/pause/step/step-over buttons; scrollbar click; section panes; monitor scrollback
- [ ] [#51](https://github.com/jawaidbazyar2/gssquared/issues/51) Printer emulation (ImageWriter II → PostScript/PDF / host print)
- [ ] [#49](https://github.com/jawaidbazyar2/gssquared/issues/49) Machine state save / load

---

## Platforms & major hardware

From [Documentation.md](Documentation.md), [AppleIIgs.md](AppleIIgs.md), [AppleIIc.md](AppleIIc.md):

- [ ] Apple IIc / IIc+ platform support
- [ ] Apple II Rev 0 / j-plus (not planned as high priority; still listed as unsupported)
- [ ] IIgs ROM03
- [X] Finish Ensoniq DOC (marked in-progress / ~half done)
- [ ] Finish / harden Zilog SCC 8530 (modem exists; control signals, reset cleanup, file device, etc. still open in [SCC8530_Serial.md](SCC8530_Serial.md))
- [-] RAMfast SCSI Interface (not started)
- [ ] Shift-key mod and Lowercase Character Generator (II+)
- [X] Crisp Lo-Res / Double Lo-Res special case in IIgs RGB mode ([AppleIIgs.md](AppleIIgs.md))
- [ ] Passport MIDI card + FluidSynth ([MIDI.md](MIDI.md))
- [ ] Advanced MMU (design agreed; not implemented) — see [MemoryProtection.md](MemoryProtection.md)

### Advanced MMU phases ([MemoryProtection.md](MemoryProtection.md))
- [ ] Phase 1: `MMU_Advanced`, shared physical map, PTE walk, `$C0Bx` registers, large pages
- [ ] Phase 2: R/W/X/PID protection + `/ABORT`; CPU opcode-vs-data distinction; TLB flush/purge hooks
- [ ] Phase 3 (optional): decoded TLB/cache if profiling justifies it
- [ ] Pick a better privileged register window than `$C0B0–$C0BF`

---

## Storage & disk

From [Documentation.md](Documentation.md), [Woz.md](Woz.md), [IWM.md](IWM.md), [DiskII.md](DiskII.md), [DevelopLog.md](DevelopLog.md):

- [ ] WOZ 2.1 support
- [X] Respect WOZ bit-timing field (Border Zone disk 2 is very slow)
- [ ] .nib 13-sector disks (size collision with DOS 3.3 .nib; need better detection)
- [ ] Successful writeback for non-WOZ formats (e.g. `.po`) where still failing
- [ ] DOS 3.2 Disk II ROM variant + import conversion for 116K images
- [X] Disk II: return floating bus on reads instead of `0xEE`
- [X] Disk II: schedule true motor-off with a timer (sometimes stays on)
- [X] If no drive enabled, deactivate phases / Q6 / Q7 logic; clear on reset
- [ ] IWM: when booting ProDOS 8 with 3.5 already inserted, Copy II Plus may not see the disk until eject/reinsert
- [ ] IWM 5.25: make `motor_on` follow enable explicitly
- [ ] Gate `fast_forward` on `motor_on` (current `lss_disk_spinning`/enable check wrong for 3.5)
- [ ] Implement Disk Speed register
- [ ] Multi-volume hard drive support
- [ ] Quarter / half track support (Locksmith nibble copy / some utils)
- [ ] DiskII volume byte from DOS 3.3 VTOC when formatting image
- [ ] pdblock3: boot from lowest numbered unit, not always unit 1
- [ ] pdblock3: ignore mount if that file is already mounted elsewhere
- [ ] Rename Generic ProDOS Block 3 to "BazFast"
- [ ] Mount error status codes for distinct user messages
- [ ] Drag/drop: save/discard prompt before remounting over modified media
- [ ] Drag/drop: smart target anywhere in window; drop onto a specific drive icon
- [ ] Stereo drive sound effects (drive 1 left / drive 2 right)
- [ ] 3.5 sound effects: low-volume spin; auto eject / insert spring-snap
- [ ] AppleDisk 5.25 effects: spin; eject/close (distinct recordings)
- [ ] Upscale disk UI assets

---

## Video & display

From [DevelopLog.md](DevelopLog.md), [AppleIIgs.md](AppleIIgs.md), [CompatibilityGS.md](CompatibilityGS.md), [UserInterface.md](UserInterface.md):

- [ ] Aspect ratio correction in VPP; correct defaults / window sizes
- [ ] Hot key for II-friendly vs GS-friendly window size toggles
- [ ] Disable invalid mixed modes (e.g. no 40-col text in double graphics)
- [ ] Split GR/text in Ludicrous Speed should be color/NTSC, not mono
- [ ] First pixel of HGR row disappears in Ludicrous Speed
- [ ] Mid-line text→hires transitions (Crazy Cycles-style) need correct monochrome bit emission
- [ ] Make SHR work again in composite
- [ ] Mono DHGR / graphics when `$C05E` active (incl. HIRES_NODELAY)
- [ ] IIe/65816: missing composite SHR; ctrl-reset should reset video registers based on scanner, not platform
- [ ] PAL //e broken (needs more scanlines; garbage/overrun)
- [ ] Implement PAL timing on GS; re-run irqtest
- [ ] Test RGB monitor colors
- [ ] Lower Planes: right border offset by 1–2 scanlines ([CompatibilityGS.md](CompatibilityGS.md))
- [ ] FTA XMAS: slight cycle timing defect on lower-right border
- [ ] Phosphor persistence effect (multi-texture fade)
- [ ] VideoScanner asserts so bad state can't crash
- [ ] Clean up leftover `set_full_frame_redraw()` once LS full-frame path is settled

---

## Audio

From [Documentation.md](Documentation.md), [MockingboardBugAnalysis.md](MockingboardBugAnalysis.md), [DevelopLog.md](DevelopLog.md), [Compatibility.md](Compatibility.md):

- [X] Finish Ensoniq fidelity (SenseiPlay / some demos still imperfect; see also Ensoniq notes)
- [-] Mockingboard mixer AND-gate (tone+noise) if still using addition ([MockingboardBugAnalysis.md](MockingboardBugAnalysis.md) Bug 5)
- [ ] Verify other MockingboardBugAnalysis findings are fixed in tree (bipolar mix, silent when both disabled, tone period/duty)
- [ ] Noise vs tone amplitude balance on Mockingboard
- [X] Mad Effects #2 still wrong (MB interrupt / VBL timing specifics)
- [ ] Speaker output delay
- [ ] Configurable speaker output rate
- [ ] Mute / drain disk sound-effect queues in Ludicrous Speed (or on ctrl-reset)
- [ ] Pause / mute audio when emulator window minimized
- [ ] Chiptunes (Skull Island, Crazy Cycles 2): stop unending warning/spam streams
- [X] Add Mockingboard cycle hook into `NClockIIgs`
- [ ] Rescue Raiders speech synth (or don't advertise speech chip) ([Compatibility.md](Compatibility.md))

---

## Input, ADB, mouse, keyboard

From [ADB.md](ADB.md), [UserInterface.md](UserInterface.md), [Networking.md](Networking.md), [DevelopLog.md](DevelopLog.md):

- [ ] Mouse interrupts only during VBL, max ~60 Hz
- [ ] Cap mouse delta (±63 counts / ~0.8")
- [X] Button 1 support / correct XY+button read order
- [ ] At RESET, disable mouse interrupts
- [ ] GS AKD bug: hold G then H, release H → AKD still set (real GS clears)
- [ ] ADB vs gamecontroller modifier keymap conflict (Alt/Win)
- [ ] Joyport workaround timing for GS reset if still flaky
- [ ] Control-Shift-2 / Control-Shift-6 → correct ASCII (Lode Runner cheat)
- [ ] Test mouse-wheel → arrow-key insertion; remove "wheel as paddle"
- [ ] Extra `0x7F` on ctrl-reset (suspect should be `0xFF` clear)
- [ ] Alien Typhoon / similar: paddle jitter when stick centered
- [ ] Gauntlet (GS): joystick won't go right/down (timing?)
- [ ] SNESMAX gamepad support (Donkey Kong mentions it)
- [ ] Write ADB test cases for recent learnings
- [ ] Add reset down/up distinction to OSD HOVER RESET button
- [ ] Right-click accelerate: restore clock correctly if GS changed speed mid-accel (`NClock` feature)

---

## Debugger, trace, external debug

From [Roadmap.md](Roadmap.md), [DebugProtocol.md](DebugProtocol.md), [DevelopLog.md](DevelopLog.md), [65816.md](65816.md):

- [ ] Ship flexible AI-drivable binary debug protocol (roadmap 0.10)
- [ ] Trace option: hide raw bytes (`PCPC: LDA $xxxx` only)
- [ ] Scrollable text widget + clickable scrollbar (click-to-jump position)
- [ ] Trace cursor → show regs / P decode for that record
- [ ] Monitor remembers bank like IIgs monitor
- [ ] Monitor command to display stack
- [ ] Address parser support for `/`
- [ ] Disassembler: set/track M/X widths (REP/SEP)
- [X] Breakpoints: PC-only / MEM-only / BOTH; value match; “write value to address”
- [X] BP on 0 must not fire on immediates like `LDA #1`
- [X] Debug frame-step: don't consume/modify normal frame-end path; interrupts shouldn't fire while CPU frozen
- [ ] Debug window near screen edge + flash → runaway flash / slow debug updates
- [ ] Store `e` bit in trace flags; polish immediate-operand display
- [ ] STP / WAI; abort/IRQ/NMI/RES stack paths if still incomplete ([65816.md](65816.md))

---

## UI / OSD / config

From [UserInterface.md](UserInterface.md), [SystemConfigTOML.md](SystemConfigTOML.md), [DevelopLog.md](DevelopLog.md):

- [X] Manage slots in Control Panel (only when system off)
- [ ] Show / manage devices on serial ports
- [X] Create / Save / Load system configs from UI (File menu + Select screen `+` / folder) — verify vs current Config Editor
- [X] On first run, copy default extra configs into user systems folder
- [ ] Cache Control Panel template texture (static chrome vs dynamic widgets)
- [ ] Platform specifies allowed CPU speed settings in OSD
- [ ] Remove "IIgs with 5.25 only" test config tile
- [ ] Professional cleanup of artwork / higher-res assets scaled down
- [ ] UI texture redraw only when dirty
- [ ] "Lots of buttons" polish pass

---

## Printers, serial, networking

From [SCC8530_Serial.md](SCC8530_Serial.md), [Parallel.md](Parallel.md), [Imagewriter.md](Imagewriter.md), [Networking.md](Networking.md), [Documentation.md](Documentation.md):

- [ ] Simulate SCC modem control signals (CTS/RTS/DSR/DTR, etc.)
- [ ] Clear SCC queues on reset
- [ ] Implement "file" serial device (complete path)
- [ ] ImageWriter II emulation
- [ ] OSD gizmo when a capture file finishes writing (path toast)
- [ ] Refactor parallel card onto threads; connect to file or ImageWriter
- [ ] Parallel: flush/close after inactivity; reopen on write; close on ctrl-reset
- [ ] Full printer→PDF pipeline (libHaru / preview window) beyond raw dump
- [ ] Uthernet II emulation
- [ ] Serial-to-telnet / Hayes path hardening as needed
- [ ] Optional FujiNet-PC / NetSIO bridge
- [ ] SMB FST via Uthernet (noted as preferred home-network path)

---

## CPU / MMU / clocks

From [CPUs.md](CPUs.md), [DevelopLog.md](DevelopLog.md), [Documentation.md](Documentation.md):

- [ ] NMOS 6502: ASL/LSR/ROL/ROR abs,X always 7 cycles
- [ ] NMOS 6502: JMP (abs) page-boundary bug + cycle count vs 65c02
- [ ] Rockwell/WDC extras if ever needed: BBR/BBS, RMB/SMB (not used on Enhanced IIe)
- [ ] Audit address-mode edge cases across 6502 / 65c02 / 65816
- [ ] Check 65c02 vs NMOS differences thoroughly
- [ ] IIe reset conditions vs Understanding the Apple IIe tables 5.4 and 7.1
- [ ] Bank latch / direct bank `$E1` tests and analysis
- [ ] MMU SoA + static aligned page table experiment (perf)
- [ ] Address mask + debug OOB warnings on MMU
- [ ] Thunderclock: interrupts, writing clock, more testing
- [ ] Fast accesses for IIgs memory softswitches ([AppleIIgs-Memory.md](AppleIIgs-Memory.md))
- [ ] Only-one-instance flags where hardware uniqueness matters (e.g. some cards)

---

## Compatibility — still noted as broken / imperfect

From [Compatibility.md](Compatibility.md) and [CompatibilityGS.md](CompatibilityGS.md) (items that still read as open at last doc update):

### Apple II
- [X] Mad Effects #2 (Mockingboard / VBL timing)
- [X] keywin.2mg (800K) does not boot
- [ ] Locksmith nibble copying / quarter-track cases
- [ ] ProTerm 2.2: loops asking for keyboard after "confirm hardware"
- [ ] Wizardry + Videx: shows only Videx screen on II+ (fine on //e)

### Apple IIgs
- [ ] Zany Golf: black playfield once in a level
- [X] Bard's Tale I/II: re-verify current status (had Ensoniq / SmartPort issues)
- [X] A2Desktop text editor/viewer crash (AUX/MAIN?)
- [X] A2Desktop with floppies enabled: hang reading floppies
- [X] SenseiPlay: songs still incorrect after Ensoniq IRQ fix
- [ ] Megademo: non-responsive after menu; fillmode defects on first screen
- [X] Nucleus / Sales Demo-class instrument artifacts (if still present)
- [ ] Photonix: direct 3.5 access / "Non bootable disk" loop
- [X] Airheart standalone on GS vs //e (5.25 bootloader / `C0EC` loop)
- [ ] DreamVoir (Golden Orchard variant) BRK after splash

---

## Cleanup / engineering hygiene

From [DevelopLog.md](DevelopLog.md), [Woz.md](Woz.md), [runloop.md](runloop.md):

- [ ] Clean memory allocation on VM shutdown
- [ ] Pull SDL init/deinit out of power on/off loop
- [ ] Improve debug emitters in diskii_controller / Floppy525
- [ ] Optimize rdpulse/wrpulse (avoid recalculating bit index every call)
- [ ] 74LS259 latch registers as bools
- [ ] `slot_rom_ptable` 16 entries for clearer loops
- [ ] Evaluate shadow+optimized vs full-frame draw
- [ ] Menu-hold / timer interaction causing audio slips (Mac menus)

---

## Sources

Primary docs consulted:

| Doc | Role |
|-----|------|
| [Roadmap.md](Roadmap.md) | Release checklist |
| [Documentation.md](Documentation.md) | Feature status matrix |
| [Compatibility.md](Compatibility.md) / [CompatibilityGS.md](CompatibilityGS.md) | Software-specific bugs |
| [DevelopLog.md](DevelopLog.md) | Long-running checkbox backlog |
| [UserInterface.md](UserInterface.md) | OSD / CP / config UI |
| [Woz.md](Woz.md) / [IWM.md](IWM.md) / [DiskII.md](DiskII.md) | Floppy / IWM |
| [AppleIIgs.md](AppleIIgs.md) / [ADB.md](ADB.md) / [SCC8530_Serial.md](SCC8530_Serial.md) | GS hardware |
| [MemoryProtection.md](MemoryProtection.md) | Advanced MMU design |
| [MockingboardBugAnalysis.md](MockingboardBugAnalysis.md) | AY-8910 bugs |
| [Networking.md](Networking.md) / [Parallel.md](Parallel.md) / [Imagewriter.md](Imagewriter.md) / [MIDI.md](MIDI.md) | Peripherals / net |
| [CPUs.md](CPUs.md) / [65816.md](65816.md) | CPU fidelity |
| [DebugProtocol.md](DebugProtocol.md) | External debugger |
| [GitHub issues](https://github.com/jawaidbazyar2/gssquared/issues) | Tracked bugs & feature requests |

When closing an item, prefer also updating the source doc checkbox (and closing the GitHub issue when applicable) so this list, the feature docs, and `gh` stay aligned.
