# SecondSight - Leave This out of the Repo for now.

So Brutal Deluxe made a game that uses Second Sight!! I had no idea. And it does not require running Z180 code. However, if we wanted to go to that length, there is this:
https://github.com/mtdev79/z180emu

## Base SS

Doing it with a Z180 emulation would be a completely different approach vs working to the API. Cogito 2 only uses the API. I seem to remember someone (Ian?) did some work with Z180 code loaded on the board, perhaps to implement sprites or some such. 

So let's look at a "naive" API-only implementation. there's the handshake and commands and register setting etc. I've dug up all the docs for that. Oh, and someone disassembled the ROM already if I have questions. 

In the emu, this will work like the Videx. I.e., when SS is put into one of the VGA modes, our frame handler draws frames and short-cuts the display frame stack. 

So we'll allocate 1MB of memory for the frame buffer. We need to support a variety of interpretations of the frame buffer. It supports:

* 8, 16, 24-bit pixels
* 8-bit pixels: palette of 24-bit colors; 640x400; 640x480; 800x600; 1024x768 resolution.
* 16-bit pixels have 15 bits of color, or 32768 direct colors; 640x400; 640x480, 800x600
* true color: 640x400; 640x480

There's also a variety of text modes.

These are selected by programming the VGA registers in a certain way. So we'll need to 'decode' those appropriately, to generate: 
    frame dimensions
    color depth

So the above tells us how to draw the frame buffer to our window. We can basically go directly from FB to window. Perhaps, we do FB -> Texture -> Window. ok, sure.

But this brings up interesting possibilities for the future: supporting SS compatibility on the various hardware cards that are coming out, e.g. Project X, VidHD2, etc etc could be doable (as long as these have enough RAM for the frame buffer!) 

Could extend the API to support 'hardware sprites'. PacMan suddenly becomes easier with 800x600: 224 x 288 doubled = 448 x 576. Fits easily (perfectly) into 800x600. Boojah. Hm, what if we could program a mode to give us 224 x 288 exactly? I don't know if that's a viable VGA mode. 

The 640x400 mode is for compatibility with AppleColor displays.
Here's another thought: we can also support memory-mapping the frame buffer directly into the 65816 address space, per Apple IIx documentation. So we have the API for setting the modes and palettes BUT then the 816 can write directly to the FB, which would speed things up. The more I delve here, the more I realize the FB approach is likely useful only/mostly to the GS. Regular IIs just don't have the RAM, typically.

The "API" is going to require supporting all the various VGA and OAK I/O registers. So we'll need to "calculate" the correct video mode from the register settings. But perhaps define a new API call to set mode line so we don't have every programmer having to bang the VGA/OAK registers directly.

Allocate three textures, one each for 8, 15, and 24 bit. Do at startup so we're not allocating/deallocating them at runtime.

OK, this works. Cards that have the resources (CPU, and memory) to implement VGA framebuffers can do the above. And, for future Apple IIx, the framebuffer can be mapped into 65816 address space. And, for tile+sprites, we extend the SS API and leverage shadowing to get data into the card faster than the regular SS interface (though that interface is not at all bad). 

Platforms I know that could handle at least some feature sets: Appletini; SS itself; Project X could up to 64K (small buffer but good for Apple II+/e also); MISTer; of course any emulator; AppleSqueezer (probably); 

## Tile+Sprite Engine 1: Console8

been reading up on how the NES works. There are two 1024 byte regions called namespaces that can be stacked horizontally or vertically, depending on the type of directional scrolling you're into. And then there is a pixel-granularity X and Y offset (they call it scroll). So if for instance you do them horizontally, You increment X by one, the display shifts by one pixel. The new needed pixel comes from the first column of tiles in the 2nd namespace. Once you've gone 8 pixels (a full tile), you can start to replace tiles in the other namespace. So each scroll only requires you to write 30 bytes to change a whole column of the screen.

You could "shake" the whole world by manipulating Y and X . 

Now the point of this isn't to exactly replicate the NES, because that is 40 year old hardware. We don't have to be constrained by the same cost limitations that platform was. But, a goal is to enable writing games in a -similar- manner.

For instance, we don't HAVE to use the same limited palette system. We could let the user define tiles and sprites using 24-bit pixel values. Tiles and sprites don't HAVE to be purely 8x8; sprites esp could be bigger. But we know that for purposes of porting NES game, that would be sufficient.

So the thing that jumps into my mind immediately is: a namespace is analagous to an Apple II text page. So that is one approach.

the NES PPU has 16K of RAM for everything. 

A tile set is 256 elements of 16 bytes each = 4K. The sprite config for 64 sprites is 256 bytes.
So now we can pack all this into one hires page and still have almost 2K left over. And could then "page flip" potentially to use the other hires page. That could be for memory layout purposes; it could be for rapidly switching tile sets for special effects or level changes or title screens what-have-you.

So we could support:
2K namespace (could use the two text pages?) (this also contains palettes)
4K tileset - this could be anywhere in the two Hires pages, so you could conceivably have up to 4 full separate tilesets.
256 bytes sprite control - this is called the object attribute memory or OAM. There are separate registers for controlling these. Guess this can be in the hires page too. 

The goal is to make what we're doing -at least as- expressive as the NES. More is better.

OK. So let the application author determine where each of these things is by programming 16 bit registers for everything.
So:
* namespace address register (16 bit)
* sprite control register (16 bit)
* tileset register (16 bit)

So they're not fixed to any one scheme.
There are various registers to program as well; scroll, etc. Memory map those also? perhaps fix those to one particular bank? So pattern to configure is:

Query SS for capability
load all the assets into RAM
set up initial screen data
configure the register address register via SS API
Enable Tile mode via SS API
**pretty screen**

ok, so that covers NES.
We could support higher resolutions: bigger namespace; or, same namespace and bigger tiles; hicolor tiles 

However, this is getting into an area where you need more memory to handle all this stuff: and so maybe this is Apple IIgs area.

Let's be clear, the NES stuff was **very** capable for an 8-bit system.

So then there's a second capability set ...

## Console16

So this will support more colors (perhaps 256 colors chosen from 15-bit hicolor); 
Tiles can be 8x8 or 16x16. 
Tilemap entries are 16 bit now, not 8 bit.
Generally, the resolution is almost the same as NES, 256x224. (maybe two tiles shorter?)
There were -some- that used a double resolution up to 512x448 or whatever. Perhaps in combo with the 16x16 tiles.

# Marketplace Methodology

By defining capabilities from the application perspective, we can hopefully drive adoption by developers, then users and hardware designers.

Ideas can be tested in emulation first before the harder/more expensive hardware gets involved.

Mike's Input:

1.  IO addresses.  What you outline below with the four registers is a pretty typical IO address pattern.   I think something like this will be part of any design.  It provides a “two way street” where data can be sent to the device (emulated or physical) and responded to.  It can also be expended with interrupts (not required, but removes polling).   It is good for “status” or “programming”, but isn’t as good for dealing with bulk data.  Moving sprite data or large bitmaps through this mechanism is slow (everything goes through a 1 byte window).

2.  Memory mapped data:  Physical devices and emulators can “mirror/shadow/snoop” memory accesses (only E0/E1 on GS) to keep a shadow copy of the A2’s memory.  This is how A2DVI works, it just watches all the 6502 memory accesses, keeps a copy and uses that data for video generation.   This makes it very efficient for dealing with large memory items (bitmaps, etc).  But it is a “one way” street.  Generally, it can't modify the data being read by the 6502 in these memory locations (assuming they are backed by RAM or MB resources).

My input is to focus on a combination of #1 and #2.   Use shadowed RAM for bulk data (screen bitmaps, sprites, location information,render lists,  etc) and use IO addresses to direct the “rendering engine” to do work with concise “commands” and responses.

Example:
In shadowed memory create a Display List (a list of commands) at $ZZZZ:  ($2000 say)
1. Clear screen command
2. Draw Text at X, Y with string “S” at shadowed address $WWWW
3. Draw Rectangle at X, Y to X1,Y2
4. Scroll screen at X,Y to X1,Y1 up 0, -10
5. End Display List

Use the mechanism you described for IO:
1. Tell the card to execute a display list at $WWWW
2. Wait for the command list to be completed (polling or IRQ).
3. Modify the command list at $WWWW or create a new one at some other location
4. Loop to 1.

If there's enough RAM you could do double-buffering a'la modern GPU to prevent screen tear. ('Present'). 

## Render List Commands

Draw Text
Rectangle (filled, hollow, pixel value)
Blit Rectangle (src texture, dst texture or screen)
Present

## Primitive Evaluation (6502 Context)

All drawing commands target the card's **linear framebuffer** (VGA-style: `address = y × pitch + x`). The host never writes pixels directly into that buffer over the bus; it issues commands and uploads bulk data via shadowed memory or texture uploads.

On a 1 MHz 6502, the card's value proposition is bus bandwidth: anything that would otherwise require the CPU to push lots of bytes or do lots of per-pixel work. Every primitive should be evaluated against one question — **does this save the CPU from touching pixels or doing per-pixel math?**

Even with a linear layout, the host still pays one slow bus transaction per pixel if it draws itself. The card runs the inner loops in fast local RAM.

### Recommended Primitives (by bang-for-buck)

#### Line drawing

Bresenham on a 1 MHz 6502 is still painful: integer math, branching, and one bus write per pixel. Address calculation in a linear buffer is trivial (`y × pitch + x`), but the loop itself is not — and every pixel still crosses the bus. A `DrawLine(x0, y0, x1, y1, color)` command is perhaps the single biggest CPU saver after blits. Extend to `DrawPolyline` (list of points) to amortize command overhead.

#### Filled and outlined polygon / triangle

Once you have lines, a triangle rasterizer opens the door to vector-style games (Elite, Star Raiders 2D). Even convex polygon fill alone is a huge win — span-filling is exactly what the host CPU is worst at, and a linear framebuffer makes span writes straightforward on the card side.

#### Circle / ellipse (filled and outlined)

Common enough in UI and games that a dedicated primitive beats decomposing into lines. Midpoint circle is trivial in hardware, brutal on a 6502.

#### Sprite blit with transparency / color key

The current Blit Rectangle is presumably a straight copy. A masked blit (skip pixels matching a key color) is what games actually need. Worth making this a separate command rather than a flag — the inner loop differs.

#### Sprite blit with transforms

At minimum: horizontal flip, vertical flip, 90° rotations. On a linear framebuffer these are nearly free in hardware (stride and step sign changes only) and save the host from storing 4× the sprite data. Arbitrary rotation and scaling are stretch goals — much more silicon, but transformative for what's possible.

#### Scrolling / viewport offset

`SetScrollOrigin(x, y)` for a tilemap or framebuffer region. Because the card owns a linear framebuffer, scrolling is just shifting the readout origin — no host-side pixel copying, no per-frame blit of the whole screen. Hardware scrolling is essentially free.

#### Tilemap blit

`DrawTilemap(tileset_texture, map_data, tile_w, tile_h, dst_rect)`. Given a tile index array and a tilesheet texture, the card renders a whole background into the linear framebuffer. This is the single biggest win for platformers and RPGs — the host updates map indices, never touches pixels.

#### Flood fill / pattern fill

Fill a bounded region with a color or a repeating pattern texture. Useful for paint programs and filled vector graphics where you'd otherwise need polygon decomposition.

#### Pattern / dithered rectangle

A variant of filled rect that takes an 8×8 (or 16×16) pattern instead of a solid color. Cheap to add, very useful for retro aesthetic where dithering substitutes for color depth.

#### Copy region (intra-framebuffer blit)

Blit from one region of the card's linear framebuffer to another, ideally with overlap handling. Enables soft-scrolling fallback, double-buffering tricks, and save/restore-background patterns for sprites — all without a round trip over the slow bus.

#### Palette / color operations (8-bit indexed modes)

`SetPalette`, `SetPaletteEntry`, `PaletteCycle(range, direction)`. For 8-bit indexed framebuffer modes, palette-cycling animations cost the host nothing if the card handles them.

#### Clipping rectangle (state, not a draw command)

`SetClipRect` — every subsequent primitive is clipped to it. Hugely simplifies windowed UIs and saves the host from pre-clipping geometry.

#### Texture management commands

`UploadTexture`, `FreeTexture`, ideally `UploadTextureCompressed` (RLE is plenty for this era's art). Bus bandwidth is the scarcest resource, so efficient uploads — and letting assets live in card RAM across frames — is foundational. RLE decode on the card side means the host pushes far fewer bytes per asset.

#### Double buffering control

`Present` implies this exists, but make it explicit: `SetRenderTarget(buffer_id)` so you can draw to offscreen buffers in card RAM, then blit or flip. Enables composite scenes built up across frames.

#### Draw text with formatting

The current DrawText is probably a single string at a position. Worth supporting: font selection (multiple uploaded font textures), proportional spacing, and a background-fill flag. Maybe a `DrawTextBox(rect, string, wrap)` for UI.

### Defer or skip

| Primitive | Reason |
|-----------|--------|
| Per-pixel `SetPixel` | Defeats the whole point — if you're talking to the card per pixel, the bus is the bottleneck again. Batch into a `DrawPoints` list instead. |
| Gradient fills | Rarely needed in this era's aesthetic; synthesizable from pattern fills. |

### Meta-suggestion: display list / command buffer mode

Upload a batch of commands once and trigger them with a single short command, possibly looping. For static or semi-static scenes (UI, background layers), this collapses many bus transactions into one and is essentially free to implement on the card side.

# Z180 Memory Map

$00/0000 - $01/FFFF - 128K SRAM

IIgs Video Shadowing; program storage

$04/0000 - $05/FFFF

A softswitch; do not modify

$06/0000 - $07/FFFF

128K EPROM

$08/0000 - $0F/FFFF

VGA Video Memory. This is 512K here, and this is banked.

## Handshaking

Pattern 1: DMA Data Transfer Commands
These use 0x01 → 0x00 handshake:

get_status:

ROM_BEEF.asm
Lines 1558-1560
                ld      a, #0
                out0    (0xA0), a
                ret
upload_chunk (used by upload_data):

ROM_BEEF.asm
Lines 1867-1869
                ld      a, #0
                out0    (0xA0), a
                ret
copy_api_data_chunk (used by set_palette):

ROM_BEEF.asm
Lines 1909-1911
                ld      a, #0
                out0    (0xA0), a
                ret

Workflow:

Write 0x01 before DMA starts
Apple II reads/writes data via C0B1/C0B2
Write 0x00 when DMA completes
Return immediately
Pattern 2: Long-Running Processing Commands
These use 0xA5 (success) / 0xA6 (error) handshake:

scroll_screen:


ROM_BEEF.asm
Lines 2032-2034
                ld      a, #0xA5
                out0    (0xA0), a
                ret
clear_screen:


ROM_BEEF.asm
Lines 2282-2284
                ld      a, #0xA5
                out0    (0xA0), a
                ret
set_video_mode (success):


ROM_BEEF.asm
Lines 3129-3130
                ld      a, #0xA5
                out0    (0xA0), a
set_video_mode (error):


ROM_BEEF.asm
Lines 3137-3138
                ld      a, #0xA6
                out0    (0xA0), a
Pattern 3: Immediate Commands
These return no explicit handshake:

set_mode:


ROM_BEEF.asm
Lines 830-831
                ret
; End of function set_mode
Why the Different Patterns?
Pattern	Handshake	Purpose
DMA Transfer
0x01→0x00
Signals when to start/stop reading/writing data
Long Operation
0xA5/0xA6
Indicates operation completion and success/failure
Immediate
None
Returns immediately, no waiting needed
Apple II Workflow Differences:
For DMA commands (upload_data, get_status, set_palette):

1. Write command → C0B0
2. Poll C0B8 until = 0x01 (ready for data)
3. Read/write data via C0B2/C0B1
4. Poll C0B8 until = 0x00 (transfer complete)
5. Done
For long-running commands (scroll_screen, clear_screen):

1. Write command → C0B0  
2. Poll C0B8 until = 0xA5 (success) or 0xA6 (error)
3. Done
For immediate commands (screen_on, screen_off):

1. Write command → C0B0
2. Done (no polling needed)
So no, not all commands use 0xA5/0xA6. DMA operations use the 0x01/0x00 pattern for flow control, while long-running operations use 0xA5/0xA6 for completion status


| command with args | command with output | bare cmd |
|-|-|-|
| get command | get command | get command |
| set up read dma to get args | execute | execute |
| execute command hook after dma complete | set up DMA to write results |
