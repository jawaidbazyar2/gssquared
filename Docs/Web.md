# Play in the browser

GSSquared can run in a modern browser (WebAssembly). The hosted build is at [https://gssquared.net/live](https://gssquared.net/live).

The page must be served with cross-origin isolation headers (`Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp`). Opening a local `GSSquared.html` via `file://` will not work. For a local build, use `python3 assets/web/serve.py` as described in [Emscripten.md](Emscripten.md).

## What works

- Built-in machines from System Select (same tiles as native).
- Keyboard, mouse, gamepad, display engines, CRT shader (when the browser GPU path is available).
- Mounting disks with **File → Drives**, clicking a Control Panel drive icon (browser file picker), or **drag-and-drop** onto a drive.

## What does not

| Limit | Why it matters |
|-------|----------------|
| **Mounts are in-memory** | Reload or close the tab and disks are gone. Writes are not saved back to your computer’s files. |
| **File protections** | .pmap files don't work |
| **No modem / SDL_net** | The Modem attachment is compiled out. Use a native build for TCP Hayes modem. |
| **No Host Folder…** | Host FST’s folder picker is native-only. |
| **No Uthernet II path** | Slirp networking is not available in the browser. |
| **No debugger window** | F10 / hover Debug does not open a second window. |

## Related

- [Storage](Storage.md) — formats and native write-back (not used on web)
- [Serial / Modem](Serial_Modem.md) — native-only modem
- [Host FST](HostFST.md) — native-only folder share
- [Emscripten.md](Emscripten.md) — how to build and deploy `/live`
