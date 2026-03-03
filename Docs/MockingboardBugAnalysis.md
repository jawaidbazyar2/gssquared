# Mockingboard AY-8910 Emulation: Bug Analysis

## Reported Symptom

When programs rapidly change each channel's volume register manually (not using
the hardware envelope feature), the channel sometimes plays **very quietly** and
sometimes at **normal volume**, switching back and forth on roughly 10-second
intervals.

---

## Summary of Findings

Five bugs were identified in the AY-8910 emulation in `mb.cpp`. Two are
**critical** and together are the most likely root cause of the reported
intermittent volume issue. The remaining three range from moderate to minor.

| # | Severity | Bug | Lines |
|---|----------|-----|-------|
| 1 | **CRITICAL** | Mixer output is bipolar (+V / -V) instead of unipolar (V / 0) | 528 |
| 2 | **CRITICAL** | Channel is silent when both tone and noise are disabled in the mixer (should be always-on) | 518-526 |
| 3 | **HIGH** | Tone counter toggles twice per period (wrong frequency, wrong duty cycle) | 326-339 |
| 4 | **MODERATE** | Tone period 0 treated as "no output" instead of equivalent to period 1 | 523 |
| 5 | **MINOR** | Mixer AND-gate not implemented: tone+noise both enabled uses addition instead of AND | 535-536 |

---

## Bug 1 (CRITICAL): Bipolar Output Model

### The Problem

The mixing code on line 528 outputs:

```cpp
float tone_contribution = tone.output ? tone.volume : -tone.volume;
```

This creates a **bipolar** square wave that swings between **+V** and **-V**.

### What the Real Chip Does

On the real AY-3-8910, the mixer/DAC output is **unipolar**. The mixer gate
controls whether the DAC outputs the channel's amplitude level or zero:

```
gate = (tone_output OR NOT tone_enable) AND (noise_output OR NOT noise_enable)
output = gate ? amplitude : 0
```

When the gate is open (1): output = the volume level (a positive voltage).
When the gate is closed (0): output = 0V (ground).

This is confirmed by the AY-3-8910 datasheet ("0 to 1 Volt" analog output
range), by the MAME reference implementation, and by die-level reverse
engineering of the chip.

### Why This Causes the Intermittent Volume Bug

For programs that rapidly write to the volume registers (the "volume DAC"
technique), the volume register values **are** the audio waveform. Each written
value represents a PCM sample of the intended sound.

**With the correct unipolar model (V / 0):**

The output waveform contains the intended audio signal at half amplitude
(the DC component that tracks the volume changes), plus a high-frequency
carrier from the tone generator. Since the carrier is typically ultrasonic,
a speaker or low-pass filter removes it, and you hear the intended audio
cleanly.

Mathematically, a 50% duty-cycle gate is equivalent to:
```
output = 0.5 * V(t) + 0.5 * square_wave(t) * V(t)
          ^^^^^^^^       ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
          baseband       AM sidebands around tone freq
          (the audio     (inaudible if tone freq is high)
           you hear)
```

**With the incorrect bipolar model (+V / -V):**

The output is `square_wave(t) * V(t)` — pure ring modulation. The baseband
term (the actual audio) is **completely absent**. What remains are AM
sidebands shifted to the tone carrier frequency:

```
output = square_wave(t) * V(t)
       = (no DC term) + sidebands at f_tone ± f_audio
```

The intended audio at baseband is gone. What you hear instead is the aliased
residue of these sidebands after digital sampling at 44.1 kHz.

**The intermittent behavior** arises because the emulator samples the chip
state at 44.1 kHz while the chip clock runs at ~63.78 kHz. The phase
relationship between the tone generator, the volume update rate (driven by
the 6502 IRQ), and the audio sample clock slowly drifts because their
frequency ratios are irrational. As the phase drifts:

- Sometimes the aliased sidebands constructively interfere, producing
  audible output at a reasonable (but incorrect) volume.
- Sometimes they destructively interfere, producing near-silence.
- The drift period depends on the exact frequency relationships and can
  easily be on the order of seconds to tens of seconds.

### Fix

Replace the bipolar output with unipolar:

```cpp
// BEFORE (wrong):
float tone_contribution = tone.output ? tone.volume : -tone.volume;

// AFTER (correct):
float tone_contribution = tone.output ? tone.volume : 0.0f;
```

A DC-blocking (high-pass) filter should be applied to the final mixed output
to remove the DC offset that the unipolar model introduces. A simple
first-order IIR high-pass at ~20 Hz is sufficient:

```cpp
// Per-chip DC blocker (run once per output sample):
float dc_blocked = sample - dc_state;
dc_state += dc_blocked * 0.001f;  // ~20 Hz cutoff at 44100 Hz
```

The same fix applies to the noise contribution on line 531:
```cpp
// BEFORE (wrong):
float noise_contribution = (chip.noise_output ? tone.volume : -tone.volume) * 0.6f;

// AFTER (correct):
float noise_contribution = (chip.noise_output ? tone.volume : 0.0f);
```

(The 0.6 scaling factor on noise is also non-authentic; the real chip treats
noise and tone amplitude identically.)

---

## Bug 2 (CRITICAL): Channel Silent When Both Tone and Noise Disabled

### The Problem

The mixer gating logic on lines 518-526:

```cpp
bool tone_enabled = !(chip.mixer_control & (1 << channel));
bool noise_enabled = !(chip.mixer_control & (1 << (channel + 3)));

bool is_tone = tone_enabled && tone.period > 0
               && (chip.registers[Ampl_A + channel] > 0);
bool is_noise = noise_enabled;

if (is_tone || is_noise) {
    // ... produce output ...
}
```

When **both** `tone_enabled` and `noise_enabled` are false (both disabled in
the mixer register R7), both `is_tone` and `is_noise` are false. The channel
contributes **zero** to the output.

### What the Real Chip Does

The AY-3-8910's mixer gate logic is:

```
gate = (tone_output OR NOT tone_enable) AND (noise_output OR NOT noise_enable)
```

When tone is disabled: `(tone_output OR 1) = 1` — tone side is forced open.
When noise is disabled: `(noise_output OR 1) = 1` — noise side is forced open.
When **both** are disabled: `1 AND 1 = 1` — the channel is **always on**.

This "always-on" behavior is the **primary method** for using the AY-8910 as
a volume DAC. Programs disable both tone and noise for a channel, then write
volume values directly. Each written value becomes the channel's output level
immediately and continuously.

### Impact

Any program using the volume-DAC technique with both tone and noise disabled
will produce **complete silence** in this emulation, instead of the intended
digitized audio. This is likely masked by Bug 1 in cases where tone is left
enabled (the bipolar distortion produces some audible output, just wrong).

### Fix

Replace the `is_tone` / `is_noise` / `if` structure with the correct gate
logic:

```cpp
for (int channel = 0; channel < 3; channel++) {
    const ToneChannel& tone = chip.tone_channels[channel];
    bool tone_dis  = (chip.mixer_control >> channel) & 1;       // 1 = disabled
    bool noise_dis = (chip.mixer_control >> (channel + 3)) & 1; // 1 = disabled

    bool tone_gate  = tone.output  || tone_dis;   // disabled = forced open
    bool noise_gate = chip.noise_output || noise_dis;

    bool gate = tone_gate && noise_gate;           // AND logic

    float channel_output = gate ? tone.volume : 0.0f;
    mixed_output[c] += channel_output;
}
```

---

## Bug 3 (HIGH): Tone Counter Double-Toggle (Wrong Frequency & Duty Cycle)

### The Problem

The tone counter on lines 326-339:

```cpp
if (channel.counter > 0) {
    channel.counter--;
    if (channel.counter == channel.period / 2) {   // <-- first toggle
        channel.output = !channel.output;
    }
} else {
    channel.counter = channel.period;               // reload
    channel.output = !channel.output;               // <-- second toggle
}
```

The output toggles at **two** points per countdown:
1. When the counter reaches `period / 2` (integer division)
2. When the counter reaches 0 and reloads

### What the Real Chip Does

The real AY-3-8910 tone counter (confirmed by MAME source, reverse-engineered
Verilog models, and the higan PSG core) works as:

```cpp
if (++counter >= period) {
    counter = 0;
    phase ^= 1;       // single toggle per period
}
```

The counter increments. When it reaches the period value, it resets and
toggles the output **once**. The full square wave cycle is `2 * period`
clock ticks (period ticks high, period ticks low). The duty cycle is
exactly 50%.

### Consequences of the Double-Toggle

| Property | Real AY-8910 | This Emulation |
|----------|-------------|----------------|
| Square wave period | `2P` clock ticks | `P + 1` clock ticks |
| Frequency | `clock / (2P)` | `clock / (P + 1)` |
| Frequency ratio | 1.0x | ~2.0x (nearly double) |
| Duty cycle (even P) | 50% | `P / (2(P+1))` ≈ 40-49% |
| Duty cycle (odd P) | 50% | 50% (correct by coincidence) |

**Example for P = 100:**
- Real frequency: 63780 / 200 = **318.9 Hz**
- Emulated frequency: 63780 / 101 = **631.5 Hz** (nearly one octave sharp)

All tones play at approximately **double** the correct pitch. The asymmetric
duty cycle for even periods also changes the harmonic content (introduces even
harmonics that shouldn't be there).

### Fix

Replace the counter logic with the standard incrementing counter:

```cpp
for (int i = 0; i < 3; i++) {
    ToneChannel& ch = chip.tone_channels[i];
    ch.counter++;
    if (ch.counter >= ch.period) {
        ch.counter = 0;
        ch.output = !ch.output;
    }
}
```

Note: period 0 should be treated as period 1 (see Bug 4).

---

## Bug 4 (MODERATE): Period 0 Treated as No Output

### The Problem

Line 523:
```cpp
bool is_tone = tone_enabled && tone.period > 0 && (...);
```

The `tone.period > 0` check means that a channel with period 0 is treated as
having no tone output, even if tone is enabled in the mixer.

### What the Real Chip Does

On the real AY-3-8910 (and YM2149), **period 0 is equivalent to period 1**.
The counter still runs and the output still toggles. This is documented in
the YM2203 datasheet and confirmed by MAME's implementation notes:

> "Also, note that period = 0 is the same as period = 1."

### Impact

Programs that set a tone period of 0 will get silence instead of a
maximum-frequency tone. This may cause missing audio in some software.

### Fix

With the corrected counter from Bug 3's fix, this is handled naturally:

```cpp
uint16_t effective_period = (ch.period == 0) ? 1 : ch.period;
ch.counter++;
if (ch.counter >= effective_period) {
    ch.counter = 0;
    ch.output = !ch.output;
}
```

And remove the `tone.period > 0` check from the mixer gate logic entirely
(it's not needed with the correct gate implementation from Bug 2's fix).

---

## Bug 5 (MINOR): Mixer AND-Gate Not Implemented

### The Problem

When both tone and noise are enabled for a channel, the emulation **adds**
their contributions (lines 535-536):

```cpp
if (is_tone && is_noise) {
    channel_output = (tone_contribution + noise_contribution);
}
```

### What the Real Chip Does

The real mixer uses an **AND** gate:

```
gate = tone_output AND noise_output
```

When both are enabled, the channel only outputs its amplitude when **both**
the tone flip-flop and the noise generator are high simultaneously. The result
is a tone that is randomly gated by the noise, creating a "buzzy" timbre.

### Impact

With the current additive approach, the combined output can reach up to 1.6x
the single-source amplitude (tone + noise * 0.6). With the correct AND gate,
the output is at most 1.0x amplitude but with a shorter duty cycle (~25% for
uncorrelated tone and noise).

This affects the timbre of sounds that use both tone and noise simultaneously
(common for explosion and percussion effects).

### Fix

This is automatically corrected by implementing the proper gate logic from
Bug 2's fix, since the AND gate applies to all enable/disable combinations.

---

## Summary: Root Cause of the Intermittent Volume

The intermittent low-volume symptom is caused primarily by **Bug 1** (bipolar
output) in combination with **Bug 2** (missing always-on behavior).

Programs that rapidly write volume values are using the AY-8910 as a DAC. They
rely on the channel being in a state where its analog output directly tracks
the written volume level. On the real chip, this works because:

1. If both tone and noise are disabled, the gate is forced permanently open,
   and every volume write immediately appears at the analog output.
2. If tone is enabled but at a high frequency, the unipolar gating preserves
   the volume signal as a DC/baseband component of the output.

In this emulation:

1. If both tone and noise are disabled, the channel is **silent** (Bug 2).
2. If tone is enabled, the bipolar ±V output performs **ring modulation**
   instead of AM gating, destroying the baseband audio signal and replacing
   it with frequency-shifted sidebands (Bug 1).

The ~10-second periodicity of the volume fluctuation comes from the slowly
drifting phase relationship between three asynchronous clocks in the system:
the AY chip clock (~63.78 kHz), the audio output sample rate (44.1 kHz),
and the 6502 CPU's volume-update IRQ rate. Because their frequency ratios
are irrational, the aliased sideband energy that leaks into the audible range
waxes and wanes over long periods.

With the correct unipolar output model and proper gate logic, the intended
audio signal is preserved at baseband regardless of phase relationships,
eliminating the intermittent volume issue entirely.

---

## Recommended Fix Priority

1. **Bug 2** — Implement the correct mixer gate logic `(tone|!tone_en) & (noise|!noise_en)`.
   This is the foundation; all other fixes layer on top.
2. **Bug 1** — Change to unipolar output (`V` or `0`, not `+V` or `-V`), add
   a DC-blocking filter on the final output.
3. **Bug 3** — Fix the tone counter to single-toggle with incrementing counter.
4. **Bug 4** — Treat period 0 as period 1.
5. **Bug 5** — Automatically resolved by Bug 2's fix.

Bugs 1 and 2 together should resolve the reported intermittent volume symptom.
Bug 3 will additionally fix all tone pitches (currently ~1 octave sharp).
