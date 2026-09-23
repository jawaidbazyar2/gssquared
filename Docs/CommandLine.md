# Command line

Native GSSquared accepts a config path and several flags. Closing the window after a CLI launch **quits the app** instead of returning to System Select.

```text
GSSquared [file.gs2|*Settings.txt] [-p platform] [-dsXdY=filename] [-s] [-g] [--debug PATH] [--no-quit-confirm]
```

On macOS the binary is inside the app bundle (`GSSquared.app/Contents/MacOS/GSSquared`). On Windows it is `GSSquared.exe` in the portable ZIP. On Linux it is the AppImage or the `GSSquared` binary from a local build.

## Config path

Pass a `.gs2` or `… Settings.txt` as the first positional argument:

```bash
GSSquared ~/Documents/GSSquared/MyIIe.gs2
GSSquared "Choplifter Settings.txt"
```

That skips System Select and boots the config immediately. Disk mounts in the file can still be overridden with `-dsXdY=` below.

## `-p N` — built-in platform

Skip System Select and boot the first built-in tile for that platform:

| `N` | Machine |
|-----|---------|
| 0 | Apple ][ |
| 1 | Apple II Plus |
| 2 | Apple IIe |
| 3 | Apple IIe Enhanced |
| 4 | Apple IIe with 65816 (VIDHD is optional in a [custom config](Cards_VIDHD.md), not this built-in) |
| 5 | Apple IIgs (ROM 01 tile; pick ROM 03 from System Select or a `.gs2`) |

```bash
GSSquared -p 3
```

## `-dsXdY=path` — mount at boot

Mount a disk image on slot **X**, drive **Y** (drives are **1-based**):

```bash
GSSquared -p 3 -ds6d1=disks/ProDOS.woz
```

This overrides a `[[storage]]` entry in a `.gs2` for the same slot and drive. The config must actually have a controller in that slot.

The debug-protocol `MOUNT` command uses **0-based** drives; the CLI does not. See [Debug Protocol](DebugProtocol.md).

## `-s` — sleep instead of busy-wait

Same as **Settings → Sleep / Busy Wait**. The emulator sleeps between frames instead of spinning, which lowers host CPU when you are not chasing maximum speed.

## `-g` — CRT shader at boot

Enables the GPU CRT effect when guest emulation starts (same as pressing **F7** with the shader off). See [Displays](Displays.md).

## `--debug PATH` / `-D PATH`

Listen for the external debug protocol on a Unix-domain socket. Desktop builds only (macOS, Windows, Linux).

```bash
GSSquared --debug /tmp/gs2.sock -p 3
```

Then use the [Python client](gs2debug.md) or the [MCP sidecar](McpServer.md). Wire format: [DebugProtocol.md](DebugProtocol.md).

## `--no-quit-confirm`

Skip the Quit confirmation modal and dirty-disk prompts on `SDL_EVENT_QUIT` (including SIGTERM, which SDL turns into quit). Intended for test harnesses. Prefer protocol `QUIT` / `c.quit()` when a debugger socket is connected.

Without this flag, closing the window or sending SIGTERM can open “Are you sure?” and leave a script blocked.

## Related

- [Selecting a System](Select.md)
- [Writing Config Files Manually](ConfigFiles.md)
- [Using the Debugger](UsingTheDebugger.md)
