/* ssi263 -- the engine's SSI-263 chip model in portable C99.

   A line-by-line port of ssi263/chip.py (engine v0.10), which stays the reference:
   research and tuning happen there, and tools/check_native_core.py holds this port to
   it (request timing and chip time exact, audio within float noise, PCM identical).
   This core is what the NVDA add-ons run, and what a SAPI or Linux front end would.

   The host writes registers (ssi263_write), watches the A/R request (ssi263_request),
   and pulls audio at its own rate (ssi263_run, ssi263_run_until_request).  The chip
   knows phoneme codes, never words; the ROM table and every parameter come from the
   caller (ssi263/native.py builds them from params.py and data/rom_bits.csv). */
#ifndef SSI263_H
#define SSI263_H

typedef float real;

#ifndef SSI263_API
#ifdef _WIN32
#define SSI263_API __declspec(dllexport)
#else
#define SSI263_API __attribute__((visibility("default")))
#endif
#endif

/* Every constant of params.py.  All doubles, so the layout is the same for every
   compiler and for Python's ctypes: switches are 0/1 (or small codes), noted below. */
typedef struct ssi263_params {
    real xck_hz;
    real frame_xck_cycles;
    real pitch_xck_div;
    real glide_xck_cycles_per_count;
    real glide_field_mult[8];
    real art_codes_per_frame[8];
    real art_amp_mult;
    real latch_round;                  /* latch_quantize: 1 "round" (half to even), 0 "floor" */
    real amp_slew_per_phoneme;
    real f_K[3];                       /* F1 F2 F3 */
    real f_C0[3];
    real f_w[3][4];
    real f4_ratio;
    real f5_ratio;
    real section_bilinear;             /* section_numerator: 1 "bilinear", 0 "allpole" */
    real bw_ratio[5];
    real nas_f2_bw_gain;
    real closure_enable;
    real closure_delay_frames;
    real closure_release_frames;
    real closure_release_b01;
    real closure_hold_delay_frames;
    real has_burst_hold;               /* burst_hold_frames is not None */
    real burst_hold_frames;
    real burst_tail_level;
    real closure_noise_at_release;
    real closure_fraction;             /* closure_timing: 1 "fraction", 0 "frames" */
    real has_closure_floor;            /* closure_floor_db is not None */
    real closure_floor_db;
    real closure_va;                   /* closure_target: 1 "va", 0 "vol" */
    real closure_ramp_ms;
    real closure_reopen;
    real pulse_fractional;             /* pulse_place: 1 "fractional", 0 "nearest" */
    real lfsr_bits;
    real noise_clock_ratio;
    real noise_shaper;                 /* 0 "flat", 1 (f/fc, B/fc) low-pass, 2 ("bp", f/fc, B/fc) */
    real noise_shaper_ratio;
    real noise_shaper_bw;
    real noise_into_f2;
    real noise_into_f5;
    real noise_route_b02;              /* noise_route: 1 "b02", 0 "fixed" */
    real noise_route_b02_w[2][2];      /* (w_F2, w_F5) for b02 = 0, 1 */
    real noise_f5_output;              /* noise_f5_point: 1 "output", 0 "input" */
    real noise_per_path;               /* noise_route_mode: 1 "per_path", 0 "switch" */
    real noise_gain;
    real hp_ratio;
    real carrier_rel_db;
    real carrier_h2_db;
    real carrier_when_powered_down;
    real output_oversample;
    real output_lowpass_hz;
    real output_gain;
    real release_lookahead;            /* a b01 stop releases early only into an open phoneme */
    real lookahead_lead_frames;        /* A/R that far ahead of the release point; writes held */
    real late_release_burst;           /* a held stop's noise is the burst at the next load */
    real fricative_precharge;          /* a stop held before a fricative charges its noise */
    real closure_onto_silence;         /* a closure loaded onto silence closes at once */
    real field_speed_mult[6];          /* per-field transition speed: F1 F2 F3 NAS VA FA */
    real glottal_n;                    /* glottal_wave: length, 1..SSI263_GLOTTAL_MAX */
    real glottal_wave[64];             /* levels per filter-clock tick; (1.0) = impulse */
    real closure_noise_lead_s;         /* bounded precharge before a release (s); < 0 = off */
    real noise_voice_swell;            /* v0.14: the noise swells with each pulse while voiced; depth, 0 = off */
} ssi263_params;

#define SSI263_GLOTTAL_MAX 64

/* One ROM entry per phoneme code 0-63, 9 bytes: F1 F2 F3 NAS VA FA (4-bit fields as
   read by the candidate decode), then the flags closure_clear (b00), class1 (b01),
   class2 (b02). */
#define SSI263_ROM_BYTES (64 * 9)

typedef struct ssi263 ssi263;

SSI263_API int ssi263_params_size(void);
/* The defaults (ssi263/params.py) and the ROM, for front ends without Python: gen_chip_defaults.py writes them
   into ssi263_defaults.h, and run_tests checks they still match. */
SSI263_API void ssi263_default_params(ssi263_params *out);
SSI263_API const unsigned char *ssi263_default_rom(void);     /* SSI263_ROM_BYTES */
SSI263_API ssi263 *ssi263_new(const ssi263_params *p, const unsigned char *rom, real out_rate);
SSI263_API void ssi263_free(ssi263 *c);

/* addr = RS2..RS0 as on the bus: 0-3 select R0-R3, 4-7 select R4. */
SSI263_API void ssi263_write(ssi263 *c, int addr, int value);
SSI263_API int ssi263_reg(const ssi263 *c, int i);
/* A/R asserted (the active-low pin is LOW): timer done, powered, and a mode with A/R.
   Under release_lookahead also ahead of a releasing stop's end; writes made then are held
   until the stop's duration is up, and A/R drops once the next phoneme is held. */
SSI263_API int ssi263_request(const ssi263 *c);
/* Chip time aligned with the audio already returned to the host. */
SSI263_API real ssi263_time(const ssi263 *c);
SSI263_API int ssi263_mode(const ssi263 *c);            /* -1 before the first CTL 1 -> 0 */
/* Host feature, not chip behaviour: the next CHANGE of pitch target lands at once. */
SSI263_API void ssi263_set_snap_pitch(ssi263 *c, int on);
SSI263_API int ssi263_get_snap_pitch(const ssi263 *c);
/* Host feature: retune the master clock without rebuilding the chip. Exact
   for everything read per-sample (filter clock, pitch period, frame/glide
   timing); a phoneme already in flight keeps the length computed when it
   started, so a mid-phoneme retune is NOT identical to a fresh ssi263_new
   at the new clock. Retuning a fresh (or idle) chip is. Added for the
   Robovox VST's coarse pitch (variable oscillator); not part of the
   chip.py port. */
SSI263_API void ssi263_set_xck(ssi263 *c, real xck);

/* Exactly n host-rate samples into out (room for n + 1).  Returns the count. */
SSI263_API long ssi263_run(ssi263 *c, long n, real *out);
/* Up to n host samples, stopping when A/R is asserted.  out needs room for n + 1. */
SSI263_API long ssi263_run_until_request(ssi263 *c, long n, real *out);
/* Advance time with no sound: timer, A/R, counters, amplitude and glide move; filter
   ringing, the held sample and the host-rate filter's history are dropped. */
SSI263_API void ssi263_skip(ssi263 *c, real seconds);

/* Internal state, for the equivalence check (see ssi263.c for the order). */
SSI263_API int ssi263_state(const ssi263 *c, real *out, int cap);

/* The host-rate stage alone (also used by ssi263/dsp.py's C backend). */
SSI263_API int ssi_fir_decimate(const real *fine, int n, const real *taps, int ntaps,
                                real *tail, int *phase, int os, real *out);
SSI263_API void ssi_pcm16(const real *y, int n, real gain, short *out);

#endif
