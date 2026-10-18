# WOZ 2.1 flux tracks

Analysis and implementation plan for playing WOZ 2.1 flux-stream tracks in GSSquared.

Spec: [WOZ Disk Image Reference 2.1](https://applesaucefdc.com/woz/reference2/) (John K. Morris, 16 June 2021).

This supersedes the sketch at the bottom of [Woz.md](Woz.md) (“FLUX Tracks”). That note proposed expanding each track to one bit per 125 ns. The player below keeps the run-length bytes and turns them into the same bit-cell stream the Disk II and IWM sequencers already consume.

## What 2.1 adds

WOZ 2.0 stores a normalized bitstream: one bit per cell, 4 µs on a 5.25″ disk and 2 µs on a 3.5″ disk. A 1 is a flux transition. A 0 is a cell with no transition. GS2 plays that stream today.

WOZ 2.1 (INFO chunk version 3) can store some or all tracks as a flux stream instead. A flux stream is the raw time between transitions, in ticks of 125 ns. It is the same encoding as A2R:

- Each byte is the number of ticks since the previous transition.
- A byte of 255 is an overflow. Add bytes until a non-255 value, then add that too. `255, 255, 10` is 520 ticks.
- A normal 4 µs gap (the 5.25″ “1” bit) is about 32 ticks. A `10` pair is about 64. A `100` triple is about 96.
- The stream is already looped. The first interval is measured from the last transition of the revolution, so wrapping the buffer does not insert or drop time.

The file stays a WOZ2 (`WOZ2` magic). Three additions:

| Piece | Role |
| --- | --- |
| INFO version 3 | Adds `FLUX Block` (uint16 at +46) and `Largest Flux Track` (uint16 at +48). Both must be non-zero or the file is not a flux image. |
| FLUX chunk | 160-byte map, same layout as TMAP. Present only for tracks stored as flux. Located at file offset `FLUX Block * 512` (the chunk id sits at that offset; the chunk occupies its own block). |
| TRKS payload | Same `TRK` descriptor as 2.0. For a flux track the `Bit Count` field is a **byte count** of the flux encoding, not a bit count. |

Map rules:

- A physical slot (5.25″ quarter-track, or 3.5″ `(track << 1) + side`) with a flux image is `0xFF` in TMAP and a TRKS index in the FLUX map.
- If a bad file has a real index in both maps, the FLUX index wins.
- `0xFF` in both maps is an empty slot. The drive still supplies random bits, and the angular position still advances. The spec’s stand-in length for an empty slot is 51,200 bit cells.

Bitstream tracks and flux tracks share one TRKS index space and may be mixed in one file. A given index is one or the other: if any FLUX-map entry points at it, that payload is flux.

`Largest Flux Track` is the block count (512-byte blocks) of the biggest flux track, for sizing a buffer. `Optimal Bit Timing` is unchanged: nominal cell size in 125 ns ticks. 32 is a 5.25″ cell (4 µs). 16 is a 3.5″ cell (2 µs).

The WRIT chunk is instructions for a device that writes a WOZ back to a real floppy. Playback does not read it.

## What GS2 does today

The container layer already knows the version-3 fields and the chunk id, and then stops.

- `woz_info_t` has `flux_block` and `largest_flux_track`. `Woz::parse_info` fills them when INFO version is ≥ 3. `Woz::dump_info` prints them.
- `WOZ_CHUNK_FLUX` (`0x58554C46`) is defined. `Woz::load` hits the default arm of the chunk switch and skips the chunk.
- `Woz::parse_trks_v2` decides which TRKS entries exist by scanning TMAP only. A flux-only slot is `0xFF` in TMAP, so its TRK is never loaded. An image that is flux on every track loads as an empty disk.
- The copy length is `(bit_count + 7) / 8`. On a flux track that field is a byte count, so this keeps one eighth of the payload and would interpret timing bytes as MSB-first bits.
- `Woz::get_track_ptr` indexes only TMAP.
- `Woz::save` / `build_trks_chunk` always write a bitstream (`(bit_count + 7) / 8` bytes, bit count in the descriptor) and never write a FLUX chunk. `build_info_chunk` will emit INFO version 3 if the flux fields are non-zero. Saving a flux image with the current writer would drop the flux map and truncate the payloads.
- There is no in-memory flag that distinguishes a flux track from a bitstream track. `woz_track_t` is `bits` plus `bit_count`.

Playback assumes every mounted track is that bitstream.

- Head position is in units of 1/128 of a bit cell (`POSITION_FP_MUL` in `Floppy_woz`).
- `Floppy525_woz::head_advance_per_cycle` is `(2 * POSITION_ADV_PER_CYCLE * 32) / optimal_bit_timing`. At the standard timing of 32 that is 4 CPU cycles per cell.
- `Floppy35_woz::head_advance_per_cycle` is `(4 * POSITION_ADV_PER_CYCLE * 16) / optimal_bit_timing`, timed on video cycles. At timing 16 that is 2 video cycles per cell (a 2 µs cell).
- `Floppy_woz::fast_forward` turns elapsed cycles into a count of whole cells, capped at 32 (`MAX_BITS_TO_SIM`).
- Disk II (`DiskII_WOZ_Controller::fast_forward`) and IWM (`IWM2::fast_forward_impl`) loop that count and call `read_pulse` / `write_pulse` once per cell. The lightweight sequencer shifts the returned bit. A 1 is treated as the read pulse for that cell.
- `read_pulse` ORs the bit into a 4-bit window and, when the window is `0000`, substitutes `get_random_bit()`. That is this emulator’s MC3470 fake-bit behavior. (The spec’s sample code also delays the output by one cell. GS2 does not. Flux should go through the same filter the bitstream path uses.)
- `Floppy_woz::update_track_ptr` keeps angular position with `head_position * new_bits / old_bits`.
- `Floppy525_woz::mount` sizes `max_tracks` from TMAP only. A disk whose outer quarter-tracks exist only in the FLUX map would clamp the head below them.
- `Floppy525_woz::write_pulse` allocates a zero bitstream when the guest writes a slot that has no track.

`Docs/Storage.md` and `Docs/Features.md` (`storage.format.woz21`) already list WOZ 2.1 as unsupported. A 2.1 file still mounts, because the magic is `WOZ2`. Bitstream tracks on it play. Flux slots read as empty and return random bits, so the copy check that needed the flux capture fails with no error at load time.

## Why a flux track cannot be played as a bitstream

The disks that need 2.1 are ones whose protection measures the time between transitions more finely than a 4 µs (or 2 µs) cell. The spec calls out on the order of a dozen such titles. Two reductions lose that measurement.

**Quantizing each gap onto the nominal cell grid.** A 32-tick gap and a 36-tick gap both fall in one cell and both become a single 1 bit. The bit stream matches. The arrival time does not. 4 ticks is 0.5 µs. Across an 8-bit nibble that is 4 µs, about 4 CPU cycles, which is enough to move a `LDA $C0EC,X` / `BPL` poll loop by one iteration.

**Expanding the track to one bit per 125 ns and still clocking the sequencer once per nominal cell.** The extra samples never reach the CPU. The sequencer only updates when `fast_forward` reports a cell, and that report is what a polling loop observes.

The CPU-visible behavior is the pair `(how many cells elapsed, what bit each cell carried)`. Flux changes both. A 36-tick gap is still one 1-bit, and that cell lasts 36 ticks instead of 32. A 64-tick gap is the two cells `1, 0` (a transition, then a cell with none). A 96-tick gap is `1, 0, 0`.

GS2 already has the right place to apply that pair. Both controllers ask the drive for a cell count and a bit per cell. The flux work belongs in the drive’s clock, not in a second sequencer.

Time base, matching the existing “4 cycles = 32 ticks” bitstream clock:

- 1 tick = 125 ns.
- 1 CPU cycle (5.25″) or 1 video cycle (3.5″) = 8 ticks.
- Nominal cell = `info.optimal_bit_timing` ticks (fallback 32 for 5.25″ and 16 for 3.5″ if the field is 0).

Do not also scale flux time by `optimal_bit_timing`. That field is the decoder’s nominal cell. The spindle speed is already the sum of the gaps. Applying it again would double-count the Border Zone-style bit-timing adjustment, which bitstream tracks still need.

## Playback design

### In memory

Add a track kind. Bitstream tracks stay as they are.

```text
enum class woz_track_kind { bitstream, flux };

struct woz_track_t {
    woz_track_kind       kind = bitstream;
    std::vector<uint8_t> bits;       // bitstream: MSB-first bits
                                     // flux: raw A2R bytes, unmodified
    uint32_t             bit_count;  // bitstream: bit count
                                     // flux: byte count (the TRK field)
};
```

On the image, store the FLUX map beside TMAP:

```text
uint8_t tmap[160];
uint8_t flux_map[160];   // 0xFF unless this slot is a flux track
bool    has_flux;        // INFO v3 and both flux fields non-zero, and the chunk parsed
```

At load, after the flux bytes are copied, decode them once into a prefix of transition times:

- Walk the bytes. Accumulate 255s. Each non-255 finishes one gap.
- `gaps[i]` is that gap in ticks. `cum[i]` is the tick time of transition `i`, measured from the start of the revolution. `cum.back()` is the revolution length, `rev_ticks`.
- A buffer that ends on 255 is truncated. Reject that track.
- `rev_ticks == 0` is an empty flux track. Treat the slot as empty.

Keep the raw bytes too, so a later save can write them back unchanged. The prefix is derived.

A dense 5.25″ revolution is about 1,600,000 ticks (200 ms at 300 RPM). An all-1s track at 32 ticks/cell is about 50,000 transitions, ~200 KB of prefix per such track. Real GCR has fewer transitions, and only the slots that failed a bitstream capture are flux. That is small next to a bitstream image.

`get_track_ptr` (and the mount-time scan of the highest occupied slot) resolves a slot in this order:

1. `flux_map[slot]` if it is not `0xFF` and in range.
2. Else `tmap[slot]` if it is not `0xFF` and in range.
3. Else empty.

If the winning index is unchanged across a seek, keep the current track pointer. That is the spec’s “matching map values, do not reload” rule, and it has to compare the winning index, not TMAP alone.

### Cell decoder

State on the drive, used only while the current track is flux:

- `head_tick`, `read_tick` — positions in ticks, modulo `rev_ticks`.
- A queue of at most `MAX_BITS_TO_SIM` already-decoded cell bits.

Decoding one gap of `G` ticks against nominal cell `N`:

```text
cells = max(1, (G + N/2) / N)    // 32 → 1, 36 → 1, 48 → 2, 64 → 2, 96 → 3
```

The transition that ends the gap is a 1. The earlier cells of that gap are 0s. Place the `cells` sample times across the gap so the last one lands on the transition (spread the remainder one tick at a time across the earlier samples). Sample values: `cells - 1` zeros, then a 1.

Examples at `N = 32`:

| Gap (ticks) | Cells | Bits | Duration |
| --- | --- | --- | --- |
| 32 | 1 | `1` | 4 cycles |
| 36 | 1 | `1` | 4.5 cycles |
| 64 | 2 | `0, 1` | 8 cycles |
| 96 | 3 | `0, 0, 1` | 12 cycles |
| 520 (`255,255,10`) | 16 | fifteen `0`s, then `1` | 65 cycles |

`fast_forward`, when the current track is flux and the spindle is on:

1. Advance `head_tick` by `elapsed_cycles * 8`, modulo `rev_ticks`.
2. Emit every sample whose time is in `(read_tick, head_tick]`, wrapping at `rev_ticks` when the interval crosses the index hole.
3. If more than 32 samples are pending, snap `read_tick` to 32 nominal cells behind `head_tick` and decode only that warmup, the same cap as the bitstream path. The lightweight sequencer’s useful state is shorter than 32 cells; decoding a full 200 ms revolution on one register access would hitch.
4. Return the number of queued samples.

`read_pulse` on a flux track pops one queued bit, runs it through the existing 4-bit `windowBits` fake-bit test, and does not advance a second clock. `fast_forward` already moved `head_tick`. `read_pulse` moves `read_tick` to that sample’s time.

Long gaps then behave like a cleaned bitstream. The decoder emits real zeros. After four zeros in the window, `read_pulse` starts returning random bits until a real 1 arrives. Flux captures are taken below the MC3470, so the fake pulses are not in the file. Always run this filter on flux output, including when INFO `cleaned` is 0. `cleaned` describes bitstream imaging, not the flux payload.

While the motor is off, do not advance `head_tick`. `fast_forward` already returns 0 in that case.

### Seeks

Keep a revolution fraction across a track change. The spec’s formula `new = current * new_length / old_length` is that fraction; the two lengths have to be in one unit.

| Leaving | Fraction |
| --- | --- |
| Bitstream | `head_position / (bit_count * POSITION_FP_MUL)` |
| Flux | `head_tick / rev_ticks` |
| Empty | `head_position / (51200 * POSITION_FP_MUL)` |

| Entering | New position |
| --- | --- |
| Bitstream | fraction × `new_bit_count` cells, in the existing 1/128-cell units |
| Flux | fraction × `rev_ticks` |
| Empty | fraction × 51,200 cells, and `read_pulse` keeps synthesizing random bits |

Switching between two slots that share a winning TRK index does not rescale.

### Writes and save

Mount any image with `has_flux` read-only.

- Force the drive write-protect sense, including when the file’s INFO write-protect flag is clear, and do not let the UI clear it.
- `write_pulse` on a flux track must not treat timing bytes as bits. Ignore the data bit, leave `modified` clear, and still consume one queued cell so a guest that writes anyway does not stall the sequencer clock. With the sense line held protected, guest software should not enter that path.
- `Floppy525_woz::write_pulse` must not allocate a fresh bitstream over a flux slot.
- `Woz::save` returns an error if `has_flux` is set. The current writer cannot round-trip the chunk. `wozutil save` on a flux image should fail the same way.

These disks are imaged originals whose protection track is the thing a copier got wrong. They are not working disks the guest needs to rewrite. A later change can allow writes on the bitstream slots of a mixed image; that requires `save` to emit the FLUX chunk on its own block, to store flux payloads with the byte count in the TRK field, and to recompute `FLUX Block` and `Largest Flux Track`. Out of scope until a mixed image actually needs it.

## Loader changes

All of this is in `src/util/woz.hpp` and `src/util/woz.cpp`.

1. Parse INFO and TMAP as now.
2. A file is a flux image only when INFO version ≥ 3 and both `flux_block` and `largest_flux_track` are non-zero. An INFO version 3 file with those fields zero is a bitstream image. Leave it on today’s path.
3. Before interpreting TRKS, read the FLUX chunk at `flux_block * 512`. Require the id `FLUX` and a 160-byte payload. If the INFO fields say flux and the chunk is missing, short, or the wrong id, fail `Woz::load`. Skipping it is what makes today’s mount look successful and then fail the protection with random bits.
4. Build the set of flux TRK indices from that map. `parse_trks_v2` takes the max index from the union of TMAP and the FLUX map. An all-flux image currently returns “no tracks” and must not.
5. For a flux index, copy `Bit Count` bytes from the track’s starting block, bounded by `block_count * 512`. For every other index, keep `(bit_count + 7) / 8`.
6. Decode gaps into the prefix. Reject a track whose decoded `rev_ticks` is 0 or whose block count exceeds `largest_flux_track`.
7. `dump_tmap` / `dump_tracks` print the FLUX map, the kind, the byte count, the transition count, and the revolution length in ticks.

`wozutil info -v` picks this up with no new command. `wozutil save` fails on a flux file until a real writer exists.

## Drive changes

`Floppy_woz` owns the clock. Disk II and IWM stay as they are: they already clock one lightweight sequencer step per cell the drive reports.

- `update_track_ptr` branches on track kind and converts the revolution fraction as above.
- `fast_forward` and `read_pulse` branch on kind. Bitstream behavior stays in the current functions.
- `tick_no_write` and `write_pulse` on a flux track advance one flux cell and do not modify bytes.
- `Floppy525_woz::mount` computes `max_tracks` from the union of both maps.
- `Floppy35_woz` uses the same player. Its nominal cell is `optimal_bit_timing` (16 at the 3.5″ default). Its clock is already video cycles, and those cycles are 8 ticks each under the same 2 µs cell. The MC3470 window is a Disk II behavior; 3.5″ flux images, if any show up, go through that same window until a 3.5″ title shows it is wrong.
- Debug lines (`Floppy525_woz::debug`, `Floppy35_woz::debug`) report kind, `rev_ticks`, and tick position when the current track is flux.

`head_advance_per_cycle` remains the bitstream formula. The flux clock is `elapsed * 8` ticks and ignores that hook.

## Implementation order

1. **Container.** Flux map, kind flag, chunk read via `flux_block`, correct byte count, prefix decode, load failure when the chunk is inconsistent, dump output. `wozutil info -v` on a real 2.1 file shows flux slots and a plausible revolution (5.25″ near 1.6e6 ticks). Bitstream WOZ 1.0 and 2.0 files still load and save.
2. **Player.** Cell decoder and the `fast_forward` / `read_pulse` branch. Synthetic gaps from the table above, driven without a full machine: 32-tick and 36-tick streams both yield all 1s, and the 36-tick stream takes 9/8 as many cycles per bit.
3. **Mount policy.** Write-protect, `save` refusal, `max_tracks` union, write path does not clobber flux bytes.
4. **Docs and status.** After playback works, update `Docs/Storage.md`, `Docs/Features.md` (`storage.format.woz21`), and `Docs/TodoList.md`.

## Tests

No WOZ unit harness exists. A small driver next to `wozutil` (or a test mode of it) can cover the decoder without booting a machine.

Decoder, nominal cell 32:

- `32` repeated → bits `1,1,1,...` and 4 cycles each.
- `36` repeated → bits `1,1,1,...` and 4.5 cycles each.
- `64` repeated → `0,1` repeating.
- `96` repeated → `0,0,1` repeating.
- `255,255,10` → 520 ticks, 16 cells, the last of them a 1.
- A stream whose tick sum is the revolution, read across the wrap, produces one continuous gap sequence. The first interval is not applied twice and no extra gap appears at the joint.

Loader:

- INFO version 3 with both flux fields 0 loads as bitstream and never looks for a FLUX chunk.
- A flux slot absent from TMAP is still loaded, and `get_track_ptr` returns it.
- A slot listed in both maps returns the flux track.
- `Bit Count` of 1000 copies 1000 bytes, not 125.
- Missing FLUX chunk with non-zero INFO fields fails `load`.

Player regression, by booting images that already work:

- A WOZ 2.0 5.25″ disk and a WOZ 2.0 3.5″ disk (read, and writeback of a bitstream image).
- A quarter-track title (Miner 2049er) and a disk with a non-default `Optimal Bit Timing` (Border Zone, disk 2). Those paths must stay on the bitstream clock.

Flux acceptance:

- Mount a real WOZ whose INFO version is 3 and whose flux fields are non-zero. Slots that are bitstream on that disk still read. The check that lives on a flux track passes.
- The image stays unmodified across the run, and unmount does not offer to overwrite it.

## Out of scope

- Writing flux back, or converting a written track into a bitstream TRK and updating the maps.
- A 2 MHz logic-state sequencer. The lightweight one-clock-per-cell model is what both controllers use for bitstream copy protection, and the flux decoder is shaped to feed it. A title that samples the read pulse inside a cell, rather than the nibble arrival time, would need that sequencer. That is a follow-up if a specific image still fails after the cell decoder matches the gap table.
- Authoring flux images, the WRIT chunk, and A2R files as a separate container.
