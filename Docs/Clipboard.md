# Clipboard

## Copy

"Copy" for now will do the following:

* Pre-allocate enough memory for conceivable Apple II bitmaps (640x216 should do it)

Call the Display Engine Copy routine

This routine (depending on display engine) will:

* Copy the screen buffer to the temporary memory after it.
* return the bitmap dimensions

"Copy" will then populate the bmp header for use when / if the clipboard callback routine is called.

## Paste

Paste is handled by the keyboard module. When a paste is done, copy the pasted string into a buffer.

**II / IIe:** each time `$C000` is read, if the strobe is clear (bit 7 = 0), inject the next character from the buffer. If the strobe is set, the guest has not processed the current keystroke. Shift+Insert, Edit → Paste Text, and debug-protocol `PASTE_TEXT` fill this buffer.

**IIgs (KeyGloo / ADB):** meter from the KeyGloo `frame_handler` (once per frame). If paste text remains and the `$C000` latch strobe is clear, inject one ASCII character via `store_key_to_buffer()` (`'\n'` becomes `'\r'`). Reset and keyboard flush abort the paste. Shift+Insert, Edit → Paste Text, and debug-protocol `PASTE_TEXT` all fill this buffer.

Implemented!

## GS/OS desk scrap ↔ host clipboard (spec)

Theoretical. Not implemented. Needs a Scrap Manager built from GS/OS source, which has not been built yet.

Edit → Copy and Edit → Paste in a GS/OS app exchange **text** with the host clipboard. PrintScreen, Shift+Insert, Edit → Paste Text, and the serial/parallel clipboard device stay as they are.

The logic lives in a modified **Scrap Manager** (tool `$16`). The tool keeps the desk scrap. On scrap calls it also reads or writes a mailbox in its own static data. The emulator finds that mailbox by signature and samples it **once per frame**, from the existing frame tick. The CPU loop does not watch toolbox dispatch. The tool does not use a WDM opcode.

On a real IIgs, or an emulator that never writes the mailbox, the host generation never moves and the tool behaves as the stock Scrap Manager. Ship the rebuilt tool on `/GS2.DRIVERS` and install it over tool `$16` in `*:System:Tools`. An emulator that does not find the signature leaves the desk scrap alone.

### Mailbox

One current entry, plus a cursor on each side.

```text
entry:                  mailbox, in the tool's static data
    magic      "GS2SCRAP"
    version
    gen        integer, starts at 0
    origin     guest | host | none
    length
    text       raw IIgs bytes (scrap type $0000), CR line endings

guest.last              in the tool: last gen absorbed
host.last               in the emulator: last gen absorbed
host.echo_gen           gen just pushed to SDL, or 0
```

Cap `text` (16KB is enough; the Scrap Manager is meant for small scraps). A larger copy still goes into the desk scrap. The host side truncates.

Writers use a seqlock: store an odd generation, copy the bytes, store the even generation. A frame boundary can split the copy. The reader retries next frame when the generation is odd or the second read disagrees.

The emulator scans guest RAM for `"GS2SCRAP"`, caches the address, and drops the cache on reset. After that, the per-frame cost is one generation compare.

The mailbox holds IIgs text. The emulator converts Macintosh Roman ↔ UTF-8 and CR ↔ LF when it talks to SDL.

### Which entry is newer

```text
newer_for_guest = entry.gen > guest.last  and  entry.origin == host
newer_for_host  = entry.gen > host.last   and  entry.origin == guest
```

A read uses the other side when that side owns a generation this side has not absorbed. A real copy mints the next generation and becomes the entry the other side will use.

### Guest copy

A real `ZeroScrap` or `PutScrap` (including `LEToScrap` and `TEToScrap`):

```text
update the desk scrap
entry.text   = desk scrap text ($0000), which may be empty
entry.origin = guest
entry.gen    = entry.gen + 1
guest.last   = entry.gen
```

### Guest paste

At the start of `GetScrap`, `GetScrapSize`, `GetScrapHandle`, `GetScrapCount`, `GetScrapState`, `GetIndScrap`, and `LoadScrap`:

```text
if newer_for_guest:
    replace the desk scrap with entry.text
        through the tool's internal store
        (this replace does not take the guest-copy path)
    guest.last = entry.gen
answer the call from the desk scrap
```

`GetScrapCount` changes when the host entry is folded in, so the Clipboard NDA and Edit → Paste follow on their next poll.

### Host

Once per frame:

```text
if newer_for_host:
    set the SDL clipboard to entry.text
    host.echo_gen = entry.gen
    host.last     = entry.gen
```

When SDL reports a clipboard change:

```text
if the new text is the echo of host.echo_gen:
    done
else:
    entry.text   = host text
    entry.origin = host
    entry.gen    = entry.gen + 1
    host.last    = entry.gen
```

The echo check is what keeps one copy from bouncing between the two clipboards.

### Startup

`ScrapStartUp` and loading `*:System:Clipboard` do not mint a generation. Booting GS/OS must not replace the host clipboard with the scrap left on disk.

`guest.last` and `host.last` start at 0. When the emulator first finds the mailbox, if the host clipboard is non-empty, it publishes that text as `origin = host`, `gen = 1`. The next guest scrap read uses it. An empty host clipboard does not publish, so a restored desk scrap is left in place until a real copy on either side.

### Overlap

Each writer publishes `gen + 1` as one sequence. If a guest copy and a host copy overlap, the write that finishes last has the higher generation. The other side uses that entry.

### Out of scope

- An app with a private scrap publishes when it calls the Scrap Manager (suspend, desk-accessory activate, `LEToScrap`, `TEToScrap`). That is the same moment other GS/OS apps can paste.
- `LEFromScrap` still rejects a desk scrap larger than 256 bytes. That limit is in LineEdit. TextEdit does not have it.
- Picture (`$0001`), TextEdit style (`$0064`), sound, and icon scraps stay in the desk scrap and are not copied into the mailbox. A guest publish still updates `entry.text` from the text scrap, so a picture-only copy clears the host text clipboard.
- Keyboard paste remains the path for ProDOS 8 and for dialogs that do not use the desk scrap.
