# firmware/vendor/ssi263 — provenance & license

These SSI-263 speech sources are vendored from the `ssi263-speech` project by
Tamas Geczy (https://github.com/lprazdnik/ssi263-speech), **MIT License**,
Copyright (c) 2026 Tamas Geczy. The full license text is in the `LICENSE` file in
this directory.

## Engine (`ssi263.c`, `ssi263.h`, `ssi263_defaults.h`, `ssi263dsp.c`)

The chip model and its host-rate output stage, taken from the upstream `src/csrc/`
native core at commit `2ada47f`. Two local changes were made for this firmware:

1. **Double-precision math ported to single precision (`real` = `float`).** The
   RP2350 has no double-precision FPU. The generated defaults stay canonical
   doubles and are cast field-by-field into the float parameter struct on load.
2. **FIR tap count made configurable** via `SSI263_FIR_TAPS_PER_OS` (default 48,
   the upstream reference value), to trade anti-alias sharpness for CPU time.

Otherwise the files are byte-faithful to upstream.

## Phoneme parameter ROM (`rom_bits.csv`)

The per-phoneme parameter ROM of the Silicon Systems SSI-263 (Votrax SC-02):
64 phonemes x 29 bits, read directly off the chip's silicon. It is **factual data
extracted from the die**, not emulator code. Taken from the upstream project's
`src/data/rom_bits.csv`; the source hash is recorded in the CSV header.

Provenance chain:
- **Die photography:** visual6502.org — Greg James photographed the SSI-263P die
  in 203 images; Christian Sattler corrected and stitched them into a single
  17265 x 14313 image.
- **ROM read:** the 1,856 ROM cells (64 x 29) were read off that die image and
  decoded as significance-interleaved 4-bit fields plus closure / class /
  fricative / voiced flags, reconciled across three independent reads.
- **Published in:** the `ssi263-speech` project (MIT), as `src/data/rom_bits.csv`.

## Glottal source — not in these files

The voice excitation (glottal source) is **not** vendored here. Upstream blends a
fraction of MAME's SC-01 glottal waveform into its voice source; to keep the
shipped firmware free of GPL code, this project supplies its own clean-room glottal
source instead (a Rosenberg flow pulse; see `OUR_GLOTTAL` in
`firmware/src/mb_speech.c`). The vendored engine takes the glottal source as data
from the caller, so no MAME-derived code ships in this firmware.
