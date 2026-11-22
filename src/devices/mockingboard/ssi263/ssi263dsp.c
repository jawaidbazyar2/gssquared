/* ssi263dsp -- the engine's host-rate output stage in C, for the NVDA add-ons.

   The chip model (ssi263/chip.py) is pure Python except for its output stage: a
   193-tap low-pass FIR at 4x the host rate, decimated.  That is ~8.5 M multiply-adds
   per second of audio -- nothing for numpy, far too much for pure Python.  The add-ons
   ship this instead of numpy, so they load in any NVDA from 2021.1 (32-bit Python 3.7,
   Windows 7) to 2026 (64-bit Python 3.13).

   Same arithmetic as ssi263/dsp.py's numpy path (np.convolve "valid", then every os-th
   sample; clip, scale, truncate to int16); the sums run in a different order, so the
   two agree to ~1e-15, not bit for bit.

   Built into ssi263.dll with the chip (build_native.py). */
#include <string.h>

#include "ssi263.h"

#define EXPORT SSI263_API

/* Streaming FIR + decimate.  ext = tail[ntaps-1] ++ fine[n];
   y[k] = sum_j ext[k+j] * taps[ntaps-1-j]  (k = 0..n-1, numpy's convolve "valid");
   writes y[phase], y[phase+os], ... to out and returns how many.  tail becomes the last
   ntaps-1 samples of ext; phase becomes (phase - n) mod os. */
EXPORT int ssi_fir_decimate(const real *fine, int n, const real *taps, int ntaps,
                            real *tail, int *phase, int os, real *out)
{
    int m = ntaps - 1, k, j, nout = 0;
    for (k = *phase; k < n; k += os) {
        real acc = 0.0;
        if (k >= m) {
            const real *x = fine + (k - m);
            for (j = 0; j <= m; j++)
                acc += x[j] * taps[m - j];
        } else {
            for (j = 0; j <= m; j++) {
                int i = k + j;
                acc += (i < m ? tail[i] : fine[i - m]) * taps[m - j];
            }
        }
        out[nout++] = acc;
    }
    if (n >= m) {
        memcpy(tail, fine + (n - m), (size_t)m * sizeof(real));
    } else {
        memmove(tail, tail + n, (size_t)(m - n) * sizeof(real));
        memcpy(tail + (m - n), fine, (size_t)n * sizeof(real));
    }
    *phase = ((*phase - n) % os + os) % os;
    return nout;
}

/* Float audio to 16-bit PCM: clip(y * gain, -1, 1) * 32767, truncated toward zero
   (numpy's astype("<i2")). */
EXPORT void ssi_pcm16(const real *y, int n, real gain, short *out)
{
    int i;
    for (i = 0; i < n; i++) {
        real v = y[i] * gain;
        if (v > 1.0)
            v = 1.0;
        else if (v < -1.0)
            v = -1.0;
        out[i] = (short)(v * 32767.0);
    }
}

/* A first-order low-pass, bilinear (as scipy's butter(1, hz, fs=...)), in place, its state
   [x1, y1] carried between blocks.  For a device's own output stage -- the Braille Lite's
   smoothing roll-off (hosts/blazie.py), not the chip. */
EXPORT void ssi_onepole(real *x, int n, real b0, real b1, real a1, real *state)
{
    int i;
    real x1 = state[0], y1 = state[1];
    for (i = 0; i < n; i++) {
        real y = b0 * x[i] + b1 * x1 - a1 * y1;
        x1 = x[i];
        y1 = y;
        x[i] = y;
    }
    state[0] = x1;
    state[1] = y1;
}
