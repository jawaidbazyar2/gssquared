# GSSquared v1.0.0

I am pleased to announce the release of **GSSquared v1.0.0**.

GSSquared is an Apple II series computer emulator.

## Supported platforms

- Apple ][
- Apple ][ Plus
- Apple //e
- Apple //e Enhanced
- Apple //e Enhanced with 65816 CPU and Super Hires Video
- Apple IIgs ROM 01
- Apple IIgs ROM 03

## Pre-built binaries

- **macOS** (Intel and Apple Silicon)
- **Windows 10+**
- **Linux AppImage** (should run on many Linux distros; tested on Ubuntu 22)

## Play in the browser

No install: the same emulator runs at [gssquared.net/live](https://gssquared.net/live).

---

This release is about a machine you can hand to someone. A `.gs2pack` is one file that holds a virtual Apple II and the disks it uses: open it and that machine boots, and when you are done, disk changes are written back into the pack. The same kind of pack can come from arQyv, including a save back to your Collection, and the web player at gssquared.net/live can start one from a link. A `.gs2` on its own is a document you can double-click. Alongside that, this release adds a Video Overlay Card, richer Second Sight text, a ThunderClock Plus, an inbound telnet modem, blank disk images from the File menu, and a long set of fixes for mouse, serial, disk, and CPU behavior.

## Features

- **GS2 packs (`.gs2pack`).** One file holds a machine and the disk images that go with it. Open it by double-click, by dragging it onto GSSquared, with **File → Launch Config…**, or from the command line. GSSquared boots that machine and, when the session ends, writes the pack back so guest disk changes stay with it. While a pack is running, the drive button lists that pack’s images, so you can swap disks that belong to the title. **Open from this computer…** still mounts a file on this computer, and that file is not stored back into the pack. macOS, Windows, and Linux open `.gs2pack` files with GSSquared. A pack may be up to 200 MB. A gzip file is not a pack.

- **arQyv packs.** The desktop app and the web player can fetch a pack that arQyv serves. Playing an item from your Collection writes disk changes back when you leave the machine, or when you choose **File → Save to Collection**. In a browser, [gssquared.net/live](https://gssquared.net/live) plays a pack from a `#pack=` link.

- **Web player speed.** The web build keeps time with the Apple II frame (about 59.92 frames per second) instead of the monitor’s refresh rate. A session is not locked to 60 Hz, and a high-refresh display does not run the machine faster.

- **`.gs2` documents.** A `.gs2` file has its own icon (a gear on color bands), and macOS, Windows, and Linux open it with GSSquared. Double-click it to launch that machine. On a IIgs, battery-backed RAM is stored in the `.gs2`; an older separate battery-RAM file is imported. Machine profiles you create live in a **GSSquared** folder in your Documents, where you can see and copy them.

- **New Disk Image.** **File → New Disk Image** writes a blank image and does not mount it. The choices are **5.25 Unformatted**, **5.25 Formatted DOS 3.3**, **5.25 Formatted ProDOS**, **3.5 Formatted ProDOS**, **32M HD Unformatted**, and **32M HD Formatted ProDOS**.

- **BlueSCSI `.hda`.** `.hda` hard-disk images are accepted and treated the same as `.hdv`.

- **Video Overlay Card.** An Apple IIgs can take a Video Overlay Card in slot 3. It shows a 640×400 super-hi-res picture by combining the two super-hi-res screens into one steady 60 Hz image.

- **Second Sight Host Text and GPU text.** With a Second Sight card, a program can show Host Text it places in memory, including tall layouts such as 80×43, 80×50, and 132×60, a mode where the card’s GPU draws the picture, and a text mode fed as a stream of words. Those pictures sit inside a border so they fill the same window area as ordinary Apple II video. Fonts load from both the older and newer places the card looks, VGA mode can be switched on or off, and a program that asks the card for its version or its capabilities gets an answer.

- **ThunderClock Plus.** The ThunderClock Plus is emulated, including its timer interrupt and the firmware utilities ProDOS uses to read the clock. ProDOS 2.4.3 no longer crashes when the card is installed.

- **Virtual modem, inbound calls.** With the Modem attached, GSSquared listens on TCP port 6502. A telnet client rings the guest. `ATA` answers, `ATS0=n` auto-answers, and `ATV0` / `ATV1` choose numeric or word result codes (including CONNECT 57600). The guest sees carrier detect for as long as that connection is up. Inbound sessions use a binary telnet mode, so ZMODEM and XMODEM transfers work.

- **Linux motherboard serial ports.** On Linux, the Host Serial list includes real PC-style serial ports (`/dev/ttyS*`). Entries that are not a real serial chip are left off the list.

- **Sirius Joyport.** **Settings → Game Controller → Joyport Controller Select** matches the card’s Left / Center / Right switch. Center lets the software choose stick 1 or 2. Either face button on a gamepad is the fire button, so an Xbox-style A button works as well as B.

- **Speed and monitor in the machine file.** Opening a `.gs2` restores the speed saved in that file (1.0, 2.8, 7.1, or 14.3 MHz) and the monitor (composite, GS RGB, green, amber, or white).

- **Apple Keys.** **Settings → Apple Keys** chooses which key on your keyboard is Open Apple: **Command = Open Apple**, **Alt = Open Apple**, or **Left Option = Open Apple**.

- **Check For Updates.** **Docs → Check For Updates** opens the updates page on gssquared.net for the version, build, and kind of computer you are running.

- **HUD.** The speed readout and the buttons beside the screen have a new look. The control that shares a host folder is a smaller **Host Folder…** button.

## Accuracy

- Switching the 65816’s X or Y register to 8-bit width clears the high byte, as on a real chip.

- Reset follows the same steps as a real 6502. The stack pointer is forced to `$00` only when the machine is powered on, not on every reset.

- A sound-chip interrupt is retired once when the guest reads its status, so a second read of the same register no longer clears a second interrupt. The GSIRC intro plays correctly.

- Bytes arriving on an IIgs serial port or a Super Serial Card are handed to the guest at the baud rate it selected. ProTERM on a IIgs at 2400 baud no longer loses characters.

- Writing a 5.25" track that has no data in the WOZ file creates that track. Locksmith 6.0 can nibble-copy *The Bilestoad* onto a blank WOZ, and the copy boots.

- A timed event is judged “already past” against the clock it was scheduled on. After a speed change, video and device events are no longer thrown away because the CPU clock had run ahead of them.

## Bug Fixes

- The Uthernet II no longer drops network data that arrives faster than the card can take in one pull. a2stream cover art and audio were garbled because of this.

- Clicks in the bars around the Apple II picture, or on the window frame, no longer move the IIgs mouse while GS/OS is tracking the pointer. Tracking stays off unless super hi-res is on.

- The pointer over the debugger, or any window other than the emulation window, is not treated as being inside the guest, and the host cursor stays visible.

- Right-clicking a machine on the System Select screen no longer launches it. Tiles start on the left button. Letting go of the right mouse button or the Insert key, when you had not pressed it to speed the machine up, no longer snaps back to an old speed (including the very fast multiplier) and no longer crashes.

- Drive sounds no longer crash GSSquared when the computer has no sound device.

- Inbound telnet sessions that the guest answers now complete the handshake the guest expects.

- Choosing a file in the web player works in Safari.

- The on-screen menu in the web player, and on Linux with a high-resolution display, is drawn at the right size. Menu text is readable, and the highlighted item follows the cursor.

- Opening an arQyv pack from the Windows build works when GSSquared is started from an MSYS2 shell.
