/* ssi263.c -- see ssi263.h.  A port of ssi263/chip.py, kept line by line so that the
   two can be read side by side; the comments that matter for faithfulness are marked
   "as chip.py".  Arithmetic follows the Python expressions' order exactly (build with
   -ffp-contract=off and, on x86, SSE2 doubles), and Python's round() -- half to even --
   is rint() here, never C's round(). */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ssi263.h"
#include "ssi263_defaults.h"

/* the generated defaults are one double per ssi263_params field (same count): a mismatch fails to compile */
typedef char ssi263_defaults_size_check[
    sizeof(ssi263_params) / sizeof(real) == sizeof(ssi263_default_params_d) / sizeof(double) ? 1 : -1];

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum { F1, F2, F3, NAS, VA, FA, NFIELDS };
#define FINE_CHUNK 2048
#define PENDING_MAX 256              /* writes held for a stop's end (as chip.py PENDING_MAX) */

typedef struct {
    real g, a1, a2, y1, y2, x1, x2;
    int zeros;       /* Resonator: bilinear numerator */
    int bandpass;    /* Bandpass (noise shaper) */
} section;

struct ssi263 {
    ssi263_params p;
    unsigned char rom[SSI263_ROM_BYTES];
    real out_rate, xck;
    int regs[5];
    int mode;                         /* -1 = None */
    int timer_done;
    int phoneme;
    real elapsed, duration;
    int target[NFIELDS];
    real cur[NFIELDS];
    int latch[NFIELDS];
    int amp_target;
    real amp_cur;
    int closing, releases;
    int released, early_req, apply_due;   /* release_lookahead (as chip.py) */
    int onto_silence;                 /* loaded onto a silent tract (closure_onto_silence) */
    int npending;
    unsigned char pend_addr[PENDING_MAX], pend_val[PENDING_MAX];
    real clo;
    real w2, w5, g2, g5;
    int imm;
    real trans;
    int trans_target;
    int snap_pitch;
    real phase, pending_pulse;
    int gw_n, shaped;                 /* glottal_wave: length, and whether it is not the impulse */
    real gw[SSI263_GLOTTAL_MAX];    /* the wave normalised to unity DC gain */
    real gl_hist[SSI263_GLOTTAL_MAX]; /* recent pulse weights, newest first */
    unsigned long long lfsr;          /* Python's int is unbounded; lfsr_bits up to 63 here */
    real noise_phase, noise_val;
    unsigned long long tick;
    section sec[5];
    section shaper;
    int shaper_on;
    real hp_x1, hp_y1;
    real out_pos, out_acc, pend_v, pend_rem;
    int os;
    real *fir, *fir_tail;
    int ntaps, dec_phase;
    real time;
    /* constants of _run */
    real hp_r, carrier[8], clo_floor;
    real fine[FINE_CHUNK];
};

/* ---- sections (as chip.py Resonator and Bandpass) ------------------------------------ */
static void set_theta(section *s, real theta, real bw_ratio)
{
    real r = exp(-M_PI * bw_ratio);
    s->a1 = 2.0 * r * cos(theta);
    s->a2 = r * r;
    s->g = (1.0 - s->a1 + s->a2) / (s->zeros ? 4.0 : 1.0);
}

static real section_step(section *s, real x)
{
    real y;
    if (s->bandpass) {
        y = s->g * (x - s->x2) + s->a1 * s->y1 - s->a2 * s->y2;
        s->x2 = s->x1;
        s->x1 = x;
    } else if (s->zeros) {
        y = s->g * (x + 2.0 * s->x1 + s->x2) + s->a1 * s->y1 - s->a2 * s->y2;
        s->x2 = s->x1;
        s->x1 = x;
    } else {
        y = s->g * x + s->a1 * s->y1 - s->a2 * s->y2;
    }
    s->y2 = s->y1;
    s->y1 = y;
    return y;
}

/* CPython's complex arithmetic, for Bandpass's gain (Objects/complexobject.c). */
typedef struct { real re, im; } cpx;
static const cpx C1 = {1.0, 0.0};

static cpx c_sum(cpx a, cpx b) { cpx r; r.re = a.re + b.re; r.im = a.im + b.im; return r; }
static cpx c_diff(cpx a, cpx b) { cpx r; r.re = a.re - b.re; r.im = a.im - b.im; return r; }
static cpx c_prod(cpx a, cpx b)
{
    cpx r;
    r.re = a.re * b.re - a.im * b.im;
    r.im = a.re * b.im + a.im * b.re;
    return r;
}
static cpx c_quot(cpx a, cpx b)
{
    cpx r;
    real abs_breal = b.re < 0 ? -b.re : b.re;
    real abs_bimag = b.im < 0 ? -b.im : b.im;
    if (abs_breal >= abs_bimag) {
        if (abs_breal == 0.0) {
            r.re = r.im = 0.0;
        } else {
            real ratio = b.im / b.re;
            real denom = b.re + b.im * ratio;
            r.re = (a.re + a.im * ratio) / denom;
            r.im = (a.im - a.re * ratio) / denom;
        }
    } else {
        real ratio = b.re / b.im;
        real denom = b.re * ratio + b.im;
        r.re = (a.re * ratio + a.im) / denom;
        r.im = (a.im * ratio - a.re) / denom;
    }
    return r;
}
static cpx c_powu(cpx x, long n)
{
    cpx r = C1, p = x;
    long mask = 1;
    while (mask > 0 && n >= mask) {
        if (n & mask)
            r = c_prod(r, p);
        mask <<= 1;
        p = c_prod(p, p);
    }
    return r;
}
static cpx c_powneg(cpx x, long n) { return c_quot(C1, c_powu(x, n)); }   /* x ** -n */
static cpx c_real(real v) { cpx r; r.re = v; r.im = 0.0; return r; }

static void bandpass_init(section *s, real ratio, real bw_ratio)
{
    real r = exp(-M_PI * bw_ratio);
    real th = 2 * M_PI * ratio;
    cpx z, zm1, zm2, num, den, h;
    s->a1 = 2.0 * r * cos(th);
    s->a2 = r * r;
    z.re = cos(th);
    z.im = sin(th);
    zm1 = c_powneg(z, 1);
    zm2 = c_powneg(z, 2);
    num = c_diff(C1, zm2);                                               /* 1 - z**-2 */
    den = c_sum(c_diff(C1, c_prod(c_real(s->a1), zm1)), c_prod(c_real(s->a2), zm2));
    h = c_quot(num, den);
    s->g = 1.0 / hypot(h.re, h.im);
    s->y1 = s->y2 = s->x1 = s->x2 = 0.0;
    s->bandpass = 1;
}

/* ---- the host-rate stage (as ssi263/dsp.py) ------------------------------------------ */
static void firwin(real *h, int numtaps, real cutoff, real fs)
{
    real c = (numtaps - 1) / 2.0, total = 0.0;
    int i;
    for (i = 0; i < numtaps; i++) {
        real x = 2.0 * cutoff / fs * (i - c);
        real s = x == 0 ? 1.0 : sin(M_PI * x) / (M_PI * x);
        h[i] = s * (0.54 - 0.46 * cos(2.0 * M_PI * i / (numtaps - 1)));
    }
    for (i = 0; i < numtaps; i++)
        total += h[i];
    for (i = 0; i < numtaps; i++)
        h[i] = h[i] / total;
}

/* ---- ROM ------------------------------------------------------------------------------ */
static const unsigned char *rom_entry(const ssi263 *c, int code) { return c->rom + 9 * (code & 0x3F); }

/* ---- chip ----------------------------------------------------------------------------- */
static int powered(const ssi263 *c) { return !(c->regs[3] & 0x80); }

/* as chip.py _open: closure-clear, voiced and noiseless */
static int entry_open(const unsigned char *e) { return e[6] && e[VA] > 0 && e[FA] == 0; }

static int pending_open(const ssi263 *c)
{
    int i;
    for (i = c->npending - 1; i >= 0; i--)
        if (c->pend_addr[i] == 0)
            return entry_open(rom_entry(c, c->pend_val[i] & 0x3F));
    return 0;
}

static const unsigned char *pending_entry(const ssi263 *c)
{
    int i;
    for (i = c->npending - 1; i >= 0; i--)
        if (c->pend_addr[i] == 0)
            return rom_entry(c, c->pend_val[i] & 0x3F);
    return NULL;
}

static int pending_has_r0(const ssi263 *c)
{
    int i;
    for (i = 0; i < c->npending; i++)
        if (c->pend_addr[i] == 0)
            return 1;
    return 0;
}

SSI263_API void ssi263_write(ssi263 *c, int addr, int value);

/* as chip.py _apply_pending: the writes held for a releasing stop's end land now */
static void apply_pending(ssi263 *c)
{
    unsigned char a[PENDING_MAX], v[PENDING_MAX];
    int i, n = c->npending;
    memcpy(a, c->pend_addr, (size_t)n);
    memcpy(v, c->pend_val, (size_t)n);
    c->npending = 0;
    c->early_req = 0;
    c->apply_due = 0;
    for (i = 0; i < n; i++)
        ssi263_write(c, a[i], v[i]);
}

static real theta(const ssi263 *c, int fi, int code)
{
    real sum = 0.0, cap, v, s;
    int i;
    for (i = 0; i < 4; i++)
        if ((code >> i) & 1)
            sum += c->p.f_w[fi][i];
    cap = c->p.f_C0[fi] + sum;
    v = c->p.f_K[fi] * cap;
    v = 0.0 > v ? 0.0 : v;                          /* max(K * cap, 0.0) */
    s = sqrt(v) / 2.0;
    s = s < 0.999 ? s : 0.999;                      /* min(0.999, ...) */
    return 2.0 * asin(s);
}

static void set_fixed_sections(ssi263 *c)
{
    set_theta(&c->sec[3], 2 * M_PI * c->p.f4_ratio, c->p.bw_ratio[3]);
    set_theta(&c->sec[4], 2 * M_PI * c->p.f5_ratio, c->p.bw_ratio[4]);
}

static void latch_all(ssi263 *c, int force)
{
    int f, changed = 0;
    for (f = 0; f < NFIELDS; f++) {
        real q = c->p.latch_round != 0.0 ? rint(c->cur[f]) : floor(c->cur[f]);
        int v = (int)q;
        v = v < 0 ? 0 : v > 15 ? 15 : v;
        if (v != c->latch[f] || force) {
            c->latch[f] = v;
            changed = 1;
        }
    }
    if (changed) {
        real b2;
        set_theta(&c->sec[0], theta(c, 0, c->latch[F1]), c->p.bw_ratio[0]);
        b2 = c->p.bw_ratio[1] * (1.0 + c->p.nas_f2_bw_gain * c->latch[NAS]);
        set_theta(&c->sec[1], theta(c, 1, c->latch[F2]), b2);
        set_theta(&c->sec[2], theta(c, 2, c->latch[F3]), c->p.bw_ratio[2]);
    }
}

static void load_phoneme(ssi263 *c)
{
    const unsigned char *e;
    int f, rate;
    real frame;
    int held = c->p.release_lookahead != 0.0 && c->p.late_release_burst != 0.0
               && c->releases && !c->released && c->clo <= 0.5;
    int prev_fa = c->target[FA];
    real pw2 = c->w2, pw5 = c->w5;
    /* nothing sounding as this phoneme loads (as chip.py, params: closure_onto_silence) */
    c->onto_silence = c->amp_cur < 0.5
                      || (c->latch[VA] == 0 && c->latch[FA] == 0 && c->g2 + c->g5 < 0.5);
    c->phoneme = c->regs[0] & 0x3F;
    e = rom_entry(c, c->phoneme);
    for (f = 0; f < NFIELDS; f++)
        c->target[f] = e[f];
    c->closing = c->p.closure_enable != 0.0 && !e[6];
    c->releases = c->closing && (e[7] == 1 || c->p.closure_release_b01 == 0.0);
    if (c->p.noise_route_b02 != 0.0) {
        c->w2 = c->p.noise_route_b02_w[e[8] ? 1 : 0][0];
        c->w5 = c->p.noise_route_b02_w[e[8] ? 1 : 0][1];
    }
    if (held && !c->closing) {
        /* the stop was held to its end: it releases here, its own noise the burst */
        real b2 = prev_fa * pw2, b5 = prev_fa * pw5;
        if (b2 > c->g2)
            c->g2 = b2;                                  /* max(g2, prev_fa * pw2) */
        if (b5 > c->g5)
            c->g5 = b5;
    }
    c->released = 0;
    c->early_req = 0;
    c->apply_due = 0;
    c->npending = 0;
    c->elapsed = 0.0;
    c->timer_done = 0;
    rate = c->regs[2] >> 4;
    frame = c->p.frame_xck_cycles * (16 - rate) / c->xck;
    if (c->mode == 1)
        c->duration = frame;
    else
        c->duration = frame * (4 - (c->regs[0] >> 6));
}

static void update_inflection(ssi263 *c)
{
    int r1 = c->regs[1], r2 = c->regs[2];
    c->imm = ((r2 >> 3) & 1) * 2048 + (r2 & 7);
    if (c->mode == 3) {
        int nw = (r1 >> 3) * 64;
        if (c->snap_pitch && nw != c->trans_target) {
            c->trans = (real)nw;
            c->snap_pitch = 0;
        }
        c->trans_target = nw;
    } else {
        c->trans_target = r1 * 8;
        c->trans = (real)c->trans_target;
    }
}

SSI263_API int ssi263_params_size(void) { return (int)sizeof(ssi263_params); }

SSI263_API void ssi263_default_params(ssi263_params *out)
{
    /* defaults are doubles; cast field-for-field into the float params struct */
    const double *src = ssi263_default_params_d;
    real *dst = (real *)out;
    int i, n = (int)(sizeof(ssi263_default_params_d) / sizeof(double));
    for (i = 0; i < n; i++)
        dst[i] = (real)src[i];
}

SSI263_API const unsigned char *ssi263_default_rom(void)
{
    return ssi263_default_rom_d;
}

SSI263_API ssi263 *ssi263_new(const ssi263_params *p, const unsigned char *rom, real out_rate)
{
    ssi263 *c = (ssi263 *)calloc(1, sizeof(ssi263));
    int i;
    if (!c)
        return NULL;
    c->p = *p;
    memcpy(c->rom, rom, SSI263_ROM_BYTES);
    c->gw_n = (int)p->glottal_n;
    if (c->gw_n < 1 || c->gw_n > SSI263_GLOTTAL_MAX)
        c->gw_n = 1;
    c->shaped = !(c->gw_n == 1 && p->glottal_wave[0] == 1.0);
    {
        real area = 0.0;                      /* summed in order, as chip.py's sum() */
        for (i = 0; i < c->gw_n; i++)
            area += p->glottal_wave[i];
        if (area == 0.0)
            area = 1.0;
        for (i = 0; i < c->gw_n; i++)
            c->gw[i] = p->glottal_wave[i] / area;
    }
    c->out_rate = out_rate;
    c->xck = p->xck_hz;
    c->regs[3] = 0x80;                               /* A: CTL set at power-up */
    c->mode = -1;
    for (i = 0; i < NFIELDS; i++)
        c->latch[i] = -1;
    c->clo = 1.0;
    c->w2 = p->noise_into_f2;
    c->w5 = p->noise_into_f5;
    c->lfsr = 1;
    c->noise_val = 1.0;
    for (i = 0; i < 5; i++)
        c->sec[i].zeros = p->section_bilinear != 0.0;
    c->shaper.zeros = p->section_bilinear != 0.0;
    c->shaper.g = 1.0;
    for (i = 0; i < 5; i++)
        c->sec[i].g = 1.0;
    c->shaper_on = p->noise_shaper != 0.0;
    if (p->noise_shaper == 2.0)
        bandpass_init(&c->shaper, p->noise_shaper_ratio, p->noise_shaper_bw);
    else if (p->noise_shaper == 1.0)
        set_theta(&c->shaper, 2 * M_PI * p->noise_shaper_ratio, p->noise_shaper_bw);
    c->os = (int)p->output_oversample;
#ifndef SSI263_FIR_TAPS_PER_OS
#define SSI263_FIR_TAPS_PER_OS 48          /* upstream reference value; lower trades sharpness for CPU */
#endif
    c->ntaps = SSI263_FIR_TAPS_PER_OS * c->os + 1;
    c->fir = (real *)calloc((size_t)c->ntaps, sizeof(real));
    c->fir_tail = (real *)calloc((size_t)c->ntaps, sizeof(real));
    if (!c->fir || !c->fir_tail) {
        ssi263_free(c);
        return NULL;
    }
    /* as chip.py: below 40 kHz hold the cutoff at 90 % of the host Nyquist (44.1 kHz unchanged) */
    firwin(c->fir, c->ntaps, p->output_lowpass_hz >= 0.5 * c->out_rate ? 0.45 * c->out_rate : p->output_lowpass_hz,
           c->out_rate * c->os);
    /* constants of _run */
    c->hp_r = 1.0 - 2.0 * M_PI * p->hp_ratio;
    {
        real c8 = 0.1 * pow(10.0, p->carrier_rel_db / 20.0);
        real c4 = c8 * pow(10.0, p->carrier_h2_db / 20.0);
        for (i = 0; i < 8; i++)
            c->carrier[i] = c8 * sin(2 * M_PI * i / 8.0) + c4 * sin(2 * M_PI * i / 4.0);
    }
    c->clo_floor = p->has_closure_floor != 0.0 ? pow(10.0, p->closure_floor_db / 20.0) : 0.0;
    set_fixed_sections(c);
    latch_all(c, 1);
    return c;
}

SSI263_API void ssi263_free(ssi263 *c)
{
    if (!c)
        return;
    free(c->fir);
    free(c->fir_tail);
    free(c);
}

SSI263_API void ssi263_write(ssi263 *c, int addr, int value)
{
    int old;
    addr &= 7;
    if (addr >= 4)
        addr = 4;
    value &= 0xFF;
    if (c->apply_due)
        apply_pending(c);
    if (c->early_req && !c->timer_done) {
        if (addr == 3 && (value & 0x80)) {
            apply_pending(c);                            /* power down: nothing waits */
        } else if (c->npending < PENDING_MAX) {
            c->pend_addr[c->npending] = (unsigned char)addr;
            c->pend_val[c->npending] = (unsigned char)value;
            c->npending++;
            return;
        } else {
            apply_pending(c);
        }
    }
    old = c->regs[addr];
    c->regs[addr] = value;
    if (addr == 0) {
        if (powered(c) && c->mode >= 0)
            load_phoneme(c);
    } else if (addr == 1 || addr == 2) {
        update_inflection(c);
    } else if (addr == 3) {
        if ((old & 0x80) && !(value & 0x80)) {
            c->mode = c->regs[0] >> 6;
            update_inflection(c);
        }
        c->amp_target = value & 0x0F;
    }
}

SSI263_API int ssi263_reg(const ssi263 *c, int i) { return (i >= 0 && i < 5) ? c->regs[i] : 0; }
SSI263_API int ssi263_request(const ssi263 *c)
{
    if (!(powered(c) && c->mode > 0))
        return 0;
    if (c->apply_due || pending_has_r0(c))
        return 0;
    return c->timer_done || c->early_req;
}
SSI263_API real ssi263_time(const ssi263 *c) { return c->time - c->pend_rem; }
SSI263_API int ssi263_mode(const ssi263 *c) { return c->mode; }
SSI263_API void ssi263_set_snap_pitch(ssi263 *c, int on) { c->snap_pitch = on != 0; }
SSI263_API int ssi263_get_snap_pitch(const ssi263 *c) { return c->snap_pitch; }
SSI263_API void ssi263_set_xck(ssi263 *c, real xck)
{
    if (c && xck > 0.0)
        c->xck = xck;
}

/* chip.py's _run and _decimate, streamed: fine samples go through the FIR in chunks,
   which gives the same output as one call (the FIR carries its tail and phase). */
static long flush(ssi263 *c, int nbuf, real *out)
{
    return ssi_fir_decimate(c->fine, nbuf, c->fir, c->ntaps, c->fir_tail, &c->dec_phase, c->os, out);
}

static long chip_run(ssi263 *c, long max_out, int stop_on_request, real *out)
{
    const ssi263_params *p = &c->p;
    long nfine = 0, nout = 0;
    int nbuf = 0;
    real to = 1.0 / (c->out_rate * c->os);
    int per_path = p->noise_per_path != 0.0;
    int hold_noise = p->closure_noise_at_release != 0.0;
    int f5_post = p->noise_f5_output != 0.0;
    int has_burst_hold = p->has_burst_hold != 0.0;
    int frac_timing = p->closure_fraction != 0.0;
    int fractional = p->pulse_fractional != 0.0;
    int clo_va = p->closure_va != 0.0;
    int carrier_off = p->carrier_when_powered_down != 0.0;
    int look = p->release_lookahead != 0.0;
    real lead = p->lookahead_lead_frames;
    int precharge = p->fricative_precharge != 0.0;
    real noise_lead = p->closure_noise_lead_s;   /* as chip.py closure_noise_lead_ms; < 0 = off */
    int lfsr_top = (int)p->lfsr_bits - 1;
    unsigned long long lfsr_mask = (1ULL << (int)p->lfsr_bits) - 1;
    real ng = p->noise_gain, gain = p->output_gain, amp_mult = p->art_amp_mult;
    real swell = p->noise_voice_swell, swell_norm = sqrt(1.0 + swell * swell / 2.0);
    real rem, v;

#define EMIT(val)                                        \
    do {                                                 \
        c->fine[nbuf++] = (val);                         \
        nfine++;                                         \
        if (nbuf == FINE_CHUNK) {                        \
            nout += flush(c, nbuf, out + nout);          \
            nbuf = 0;                                    \
        }                                                \
    } while (0)

    /* finish the held sample a previous call stopped inside */
    rem = c->pend_rem;
    v = c->pend_v;
    while (rem > 0.0 && nfine < max_out) {
        real take = to - c->out_pos < rem ? to - c->out_pos : rem;       /* min(rem, ...) */
        c->out_acc += v * take;
        c->out_pos += take;
        rem -= take;
        if (c->out_pos >= to - 1e-15) {
            EMIT(c->out_acc / to);
            c->out_acc = 0.0;
            c->out_pos = 0.0;
        }
    }
    c->pend_rem = rem;
    while (nfine < max_out) {
        real fc, dt;
        int pw;
        if (c->apply_due)
            apply_pending(c);                            /* as a host writing at this tick boundary */
        if (stop_on_request && ssi263_request(c))
            break;
        fc = c->xck / (2.0 * (256 - c->regs[4]));
        dt = 1.0 / fc;
        pw = powered(c);
        v = 0.0;
        if (pw) {
            real frame, step, d, cstep, scale, rel, cdel, I, period, inc, pulse, ph, x, nz, n2, n5, hp, amp;
            int f, n, rel_now;
            const int *la;
            /* -- controller: timer, transition counters, glide -- */
            if (c->mode >= 0) {
                c->elapsed += dt;
                if (!c->timer_done && c->elapsed >= c->duration) {
                    c->timer_done = 1;
                    if (c->npending)
                        c->apply_due = 1;
                }
            }
            frame = p->frame_xck_cycles * (16 - (c->regs[2] >> 4)) / c->xck;
            step = p->art_codes_per_frame[(c->regs[3] >> 4) & 7] * dt / frame;
            for (f = 0; f < NFIELDS; f++) {
                d = c->target[f] - c->cur[f];
                if (d != 0.0) {
                    real st = ((f == VA || f == FA) ? step * amp_mult : step) * p->field_speed_mult[f];
                    c->cur[f] = fabs(d) <= st ? (real)c->target[f] : c->cur[f] + copysign(st, d);
                }
            }
            if (c->duration > 0) {
                real astep = 15.0 * p->amp_slew_per_phoneme * dt / c->duration;
                d = c->amp_target - c->amp_cur;
                if (d != 0.0)
                    c->amp_cur = fabs(d) <= astep ? (real)c->amp_target : c->amp_cur + copysign(astep, d);
            }
            d = c->trans_target - c->trans;
            if (d != 0.0) {
                int rate = c->regs[2] >> 4;
                real gstep = (p->glide_field_mult[c->regs[1] & 7] * c->xck
                                / (p->glide_xck_cycles_per_count * (16 - rate))) * dt;
                c->trans = fabs(d) <= gstep ? (real)c->trans_target : c->trans + copysign(gstep, d);
            }
            cstep = dt * 1000.0 / p->closure_ramp_ms;
            scale = (frac_timing && frame > 0) ? (c->duration / (4.0 * frame)) : 1.0;
            rel = c->releases ? p->closure_release_frames * frame * scale : 0.0;
            cdel = c->releases ? p->closure_delay_frames : p->closure_hold_delay_frames;
            if (c->onto_silence && p->closure_onto_silence != 0.0)
                cdel = 0.0;
            rel_now = rel > 0 && c->elapsed >= c->duration - rel;
            if (look && c->releases) {
                /* ask for the next phoneme ahead of the release point; release only into an
                   open one (as chip.py, params: release_lookahead) */
                if (c->mode > 0 && !c->early_req && !c->timer_done
                        && c->elapsed >= c->duration - rel - lead * frame * scale)
                    c->early_req = 1;
                if (rel_now && !c->released)
                    c->released = pending_open(c);
                rel_now = rel_now && c->released;
            }
            if (c->closing && c->elapsed >= cdel * frame * scale && !rel_now) {
                real t = c->clo - cstep;
                c->clo = t > c->clo_floor ? t : c->clo_floor;          /* max(clo_floor, ...) */
            } else if ((p->closure_reopen != 0.0 || !c->closing || rel_now) && c->clo < 1.0) {
                real t = c->clo + cstep;
                c->clo = t < 1.0 ? t : 1.0;                              /* min(1.0, ...) */
            }
            latch_all(c, 0);
            la = c->latch;
            /* -- sources -- */
            I = c->imm + c->trans;
            period = p->pitch_xck_div * (4096.0 - I) / c->xck;
            inc = dt / period;
            pulse = c->pending_pulse;
            c->pending_pulse = 0.0;
            ph = c->phase + inc;
            n = (int)ph;
            if (n) {
                int k;
                for (k = 1; k <= n; k++) {
                    if (fractional) {
                        real a = (k - c->phase) / inc;     /* where in this tick crossing k falls */
                        pulse += 1.0 - a;
                        c->pending_pulse += a;
                    } else {
                        pulse += 1.0;
                    }
                }
                ph -= n;
            }
            c->phase = ph;
            if (c->shaped) {
                /* the source wave, restarted by every pulse: a FIR over the pulse weights */
                real acc = 0.0;
                int j;
                memmove(&c->gl_hist[1], &c->gl_hist[0], sizeof(real) * (size_t)(c->gw_n - 1));
                c->gl_hist[0] = pulse;
                for (j = 0; j < c->gw_n; j++)
                    acc += c->gw[j] * c->gl_hist[j];
                pulse = acc;
            }
            x = pulse * la[VA] / 15.0;
            if (clo_va)
                x *= c->clo;
            c->noise_phase += p->noise_clock_ratio;
            while (c->noise_phase >= 1.0) {
                unsigned long long bit;
                c->noise_phase -= 1.0;
                bit = ((c->lfsr >> lfsr_top) ^ (c->lfsr >> (lfsr_top - 1))) & 1;
                c->lfsr = ((c->lfsr << 1) | bit) & lfsr_mask;
                c->noise_val = bit ? 1.0 : -1.0;
            }
            if (per_path) {
                real astp = step * amp_mult;
                real t2 = c->target[FA] * c->w2, t5 = c->target[FA] * c->w5;
                int rel_noise = c->elapsed >= c->duration - p->closure_release_frames * frame * scale;
                if (look && c->releases)
                    rel_noise = rel_noise && c->released;
                if (hold_noise && c->closing && (!c->releases || !rel_noise)) {
                    t2 = t5 = 0.0;
                    if (look && precharge && c->releases && !c->released
                            && c->elapsed >= c->duration - p->closure_release_frames * frame * scale) {
                        /* the next phoneme is a fricative: its noise builds behind the closed gate */
                        const unsigned char *pe = pending_entry(c);
                        if (pe && pe[6] && pe[FA] > 0) {
                            real w2 = c->w2, w5 = c->w5;
                            if (p->noise_route_b02 != 0.0) {
                                w2 = p->noise_route_b02_w[pe[8] ? 1 : 0][0];
                                w5 = p->noise_route_b02_w[pe[8] ? 1 : 0][1];
                            }
                            t2 = pe[FA] * w2;
                            t5 = pe[FA] * w5;
                        }
                    } else if (noise_lead >= 0.0 && look && c->releases && !c->released
                               && c->elapsed >= c->duration - p->closure_release_frames * frame * scale - noise_lead
                               && c->clo <= 0.0 && pending_open(c)) {
                        /* a bounded precharge: the stop's own noise builds behind the closed gate over its last
                           noise_lead before the release point (Astra, Reply 59; Tomi's ear, 2026-09-27) */
                        t2 = c->target[FA] * c->w2;
                        t5 = c->target[FA] * c->w5;
                    }
                } else if (has_burst_hold && c->closing && c->releases
                           && c->elapsed >= c->duration
                              - (p->closure_release_frames - p->burst_hold_frames) * frame * scale) {
                    t2 = t2 * p->burst_tail_level;
                    t5 = t5 * p->burst_tail_level;
                }
                d = t2 - c->g2;
                c->g2 = fabs(d) <= astp ? t2 : c->g2 + copysign(astp, d);
                d = t5 - c->g5;
                c->g5 = fabs(d) <= astp ? t5 : c->g5 + copysign(astp, d);
                nz = c->noise_val * ng / 15.0;
                n2 = c->g2;
                n5 = c->g5;
            } else {
                nz = c->noise_val * ng * la[FA] / 15.0;
                n2 = c->w2;
                n5 = c->w5;
            }
            if (swell != 0.0 && la[VA] > 0)
                /* the noise swells with each glottal pulse while voiced (chip.py noise_voice_swell, v0.14) */
                nz = nz * (1.0 + swell * cos(2 * M_PI * c->phase)) / swell_norm;
            if (c->shaper_on)
                nz = section_step(&c->shaper, nz);
            /* -- the cascade: F1 -> F2 (+noise) -> F3 -> F4 -> F5 (+noise) -- */
            if (f5_post)
                x = section_step(&c->sec[4], section_step(&c->sec[3], section_step(&c->sec[2],
                        section_step(&c->sec[1], section_step(&c->sec[0], x) + n2 * nz)))) + n5 * nz;
            else
                x = section_step(&c->sec[4], section_step(&c->sec[3], section_step(&c->sec[2],
                        section_step(&c->sec[1], section_step(&c->sec[0], x) + n2 * nz))) + n5 * nz);
            /* -- high-pass with volume, then S/H -- */
            hp = x - c->hp_x1 + c->hp_r * c->hp_y1;
            c->hp_x1 = x;
            c->hp_y1 = hp;
            amp = rint(c->amp_cur) / 15.0;                   /* round(): half to even */
            v = gain * hp * amp * (clo_va ? 1.0 : c->clo);
        }
        if (pw || carrier_off)
            v += c->carrier[c->tick & 7];
        c->tick += 1;
        /* -- area-sample the held value to the host rate -- */
        rem = dt;
        while (rem > 0.0) {
            real take;
            if (nfine >= max_out) {
                c->pend_v = v;
                c->pend_rem = rem;
                break;
            }
            take = to - c->out_pos < rem ? to - c->out_pos : rem;
            c->out_acc += v * take;
            c->out_pos += take;
            rem -= take;
            if (c->out_pos >= to - 1e-15) {
                EMIT(c->out_acc / to);
                c->out_acc = 0.0;
                c->out_pos = 0.0;
            }
        }
        c->time += dt;
    }
    if (nbuf)
        nout += flush(c, nbuf, out + nout);
    return nout;
#undef EMIT
}

SSI263_API long ssi263_run(ssi263 *c, long n, real *out)
{
    return chip_run(c, n * c->os, 0, out);
}

SSI263_API long ssi263_run_until_request(ssi263 *c, long n, real *out)
{
    return chip_run(c, n * c->os, 1, out);
}

SSI263_API void ssi263_skip(ssi263 *c, real seconds)
{
    const ssi263_params *p = &c->p;
    int i, end_now = 0;
    if (seconds <= 0)
        return;
    if (c->apply_due)
        apply_pending(c);
    if (powered(c) && c->mode >= 0) {
        real left = c->duration - c->elapsed, frame, step, d, early_left = 0.0;
        int f, has_early = 0;
        if (p->release_lookahead != 0.0 && c->releases && c->mode > 0 && !c->early_req && !c->timer_done) {
            real fr = p->frame_xck_cycles * (16 - (c->regs[2] >> 4)) / c->xck;
            real sc = (p->closure_fraction != 0.0 && fr > 0) ? (c->duration / (4.0 * fr)) : 1.0;
            early_left = c->duration - p->closure_release_frames * fr * sc
                         - p->lookahead_lead_frames * fr * sc - c->elapsed;
            has_early = 1;
        }
        if (has_early && seconds >= early_left) {
            seconds = early_left > 0.0 ? early_left : 0.0;          /* max(early_left, 0.0) */
            c->early_req = 1;
        } else if (!c->timer_done && seconds >= left) {
            seconds = left > 0.0 ? left : 0.0;                      /* max(left, 0.0) */
            c->timer_done = 1;
            end_now = c->npending > 0;
        }
        c->elapsed += seconds;
        frame = p->frame_xck_cycles * (16 - (c->regs[2] >> 4)) / c->xck;
        step = p->art_codes_per_frame[(c->regs[3] >> 4) & 7] * seconds / frame;
        for (f = 0; f < NFIELDS; f++) {
            real st = ((f == VA || f == FA) ? step * p->art_amp_mult : step) * p->field_speed_mult[f];
            d = c->target[f] - c->cur[f];
            c->cur[f] = fabs(d) <= st ? (real)c->target[f] : c->cur[f] + copysign(st, d);
        }
        c->g2 = c->target[FA] * c->w2;
        c->g5 = c->target[FA] * c->w5;
        if (c->duration > 0) {
            real astep = 15.0 * p->amp_slew_per_phoneme * seconds / c->duration;
            d = c->amp_target - c->amp_cur;
            c->amp_cur = fabs(d) <= astep ? (real)c->amp_target : c->amp_cur + copysign(astep, d);
        }
        c->clo = c->closing ? 0.0 : 1.0;
        d = c->trans_target - c->trans;
        if (d != 0.0) {
            int rate = c->regs[2] >> 4;
            real g = (p->glide_field_mult[c->regs[1] & 7] * c->xck
                        / (p->glide_xck_cycles_per_count * (16 - rate))) * seconds;
            c->trans = fabs(d) <= g ? (real)c->trans_target : c->trans + copysign(g, d);
        }
        latch_all(c, 0);
    }
    /* skipped output is discarded: no ringing or held samples survive it */
    for (i = 0; i < 5; i++)
        c->sec[i].y1 = c->sec[i].y2 = c->sec[i].x1 = c->sec[i].x2 = 0.0;
    c->shaper.y1 = c->shaper.y2 = c->shaper.x1 = c->shaper.x2 = 0.0;
    c->hp_x1 = c->hp_y1 = 0.0;
    c->pending_pulse = 0.0;
    memset(c->gl_hist, 0, sizeof c->gl_hist);
    memset(c->fir_tail, 0, sizeof(real) * (size_t)c->ntaps);
    c->pend_v = c->pend_rem = 0.0;
    c->out_acc = c->out_pos = 0.0;
    c->time += seconds;
    if (end_now)
        apply_pending(c);
}

/* elapsed duration trans trans_target amp_cur clo g2 g5 phase pending_pulse noise_phase
   lfsr timer_done mode time, then cur[6], latch[6], then released early_req apply_due
   npending onto_silence */
SSI263_API int ssi263_state(const ssi263 *c, real *out, int cap)
{
    real s[32];
    int i, n = 0;
    s[n++] = c->elapsed;
    s[n++] = c->duration;
    s[n++] = c->trans;
    s[n++] = c->trans_target;
    s[n++] = c->amp_cur;
    s[n++] = c->clo;
    s[n++] = c->g2;
    s[n++] = c->g5;
    s[n++] = c->phase;
    s[n++] = c->pending_pulse;
    s[n++] = c->noise_phase;
    s[n++] = (real)c->lfsr;
    s[n++] = c->timer_done;
    s[n++] = c->mode;
    s[n++] = c->time;
    for (i = 0; i < NFIELDS; i++)
        s[n++] = c->cur[i];
    for (i = 0; i < NFIELDS; i++)
        s[n++] = c->latch[i];
    s[n++] = c->released;
    s[n++] = c->early_req;
    s[n++] = c->apply_due;
    s[n++] = c->npending;
    s[n++] = c->onto_silence;
    for (i = 0; i < n && i < cap; i++)
        out[i] = s[i];
    return n;
}
