#pragma once

/**
 * Shared pieces for the NTSC prototype tools (ntsc1120, ntscview):
 * HGR / DHGR bitstream expansion, the production-LUT 560 decoder, and a
 * continuous held-dot kernel that can be evaluated at any horizontal position.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "display/ntsc.hpp"
#include "display/OEVector.h"

extern RGBA_t g_hgr_LUT[4][(1 << ((NUM_TAPS * 2) + 1))];

namespace ntsc_proto {

static constexpr int kDots = 560;
static constexpr int kLines = 192;
static constexpr float kFsc = 315.0f / 88.0f * 1e6f;
static constexpr float kFsc4 = 4.0f * kFsc;

struct Bitstream {
    uint8_t dots[kLines][kDots];
    uint8_t phase_offset = 0;
};

/** RGB line on the 560-dot grid, values 0..255. */
using Line560 = float[kDots][3];

// ---------------------------------------------------------------------------
// Bitstream generation (same construction as AppleII_Context in
// src/devices/displaypp/generate/AppleII.cpp)

inline uint16_t hgr_row_offset(int y) {
    return (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28;
}

inline void expand_hgr(const uint8_t *page, Bitstream &bs) {
    static uint8_t font[512 * 16];
    for (int i = 0; i < 512; i++) {
        uint8_t value = (i & 0x7f) << 1 | (i >> 8);
        int delay = (i & 0x80) ? 1 : 0;
        for (int x = 0; x < 16; x++) {
            font[i * 16 + x] = (value >> ((x + 2 - delay) >> 1)) & 1;
        }
    }
    bs.phase_offset = 0;
    for (int y = 0; y < kLines; y++) {
        const uint8_t *d = page + hgr_row_offset(y);
        uint8_t lastByte = 0;
        uint8_t *p = bs.dots[y];
        for (int x = 0; x < 40; x++) {
            uint8_t byte = d[x];
            const uint8_t *f = &font[(byte | ((lastByte & 0x40) << 2)) * 16];
            for (int i = 0; i < 14; i++) *p++ = f[i];
            lastByte = byte;
        }
    }
}

inline void expand_dhgr(const uint8_t *aux, const uint8_t *main, Bitstream &bs) {
    bs.phase_offset = 1;
    for (int y = 0; y < kLines; y++) {
        uint16_t off = hgr_row_offset(y);
        uint8_t *p = bs.dots[y];
        for (int x = 0; x < 40; x++) {
            uint8_t a = aux[off + x];
            uint8_t m = main[off + x];
            for (int i = 0; i < 7; i++) { *p++ = a & 1; a >>= 1; }
            for (int i = 0; i < 7; i++) { *p++ = m & 1; m >>= 1; }
        }
    }
}

/**
 * Load an HGR (8K, 8184-byte files padded) or DHGR (16K: aux then main) page.
 * force_mode: 0 auto by size, 1 HGR, 2 DHGR. Returns false if unreadable.
 */
inline bool load_page(const std::string &path, int force_mode, Bitstream &bs, bool &dhgr, size_t &nbytes) {
    std::vector<uint8_t> data(16384, 0);
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    nbytes = fread(data.data(), 1, data.size(), f);
    fclose(f);
    dhgr = force_mode ? (force_mode == 2) : (nbytes >= 16384);
    if (dhgr) expand_dhgr(data.data(), data.data() + 0x2000, bs);
    else expand_hgr(data.data(), bs);
    return true;
}

inline std::string base_name(const std::string &path) {
    std::string name = path.substr(path.find_last_of('/') + 1);
    if (size_t dot = name.find_last_of('.'); dot != std::string::npos && dot > 0) name = name.substr(0, dot);
    return name;
}

// ---------------------------------------------------------------------------
// 560 decode through the production LUT (mirrors NTSC560::render)

inline void decode_lut560(const RGBA_t (*lut)[1 << ((NUM_TAPS * 2) + 1)], const uint8_t *dots,
                          uint8_t phase_offset, Line560 &out) {
    uint32_t bits = 0;
    for (int i = 0; i < NUM_TAPS; i++) {
        bits >>= 1;
        if (dots[i]) bits |= 1u << (NUM_TAPS * 2);
    }
    for (int x = 0; x < kDots; x++) {
        bits >>= 1;
        if (x < kDots - NUM_TAPS && dots[x + NUM_TAPS]) bits |= 1u << (NUM_TAPS * 2);
        RGBA_t c = lut[(phase_offset + x) % 4][bits];
        out[x][0] = c.r;
        out[x][1] = c.g;
        out[x][2] = c.b;
    }
}

inline float lanczos3(float x) {
    if (x == 0.0f) return 1.0f;
    if (std::fabs(x) >= 3.0f) return 0.0f;
    float px = (float)M_PI * x;
    return 3.0f * sinf(px) * sinf(px / 3.0f) / (px * px);
}

// ---------------------------------------------------------------------------
// Continuous held-dot kernel.
//
// K(d, p) is the Y/I/Q contribution of a set dot with carrier phase p whose
// centre lies a distance d (in dots) before the output point. It is the
// windowed-sinc filter times the carrier, integrated across the dot's width,
// i.e. the model of generate_held_weights() in src/display/filters.cpp
// evaluated at fractional offsets. The table holds SUB+1 fractional positions
// so every output position u = base + si/SUB needs no carry.
//
// With a trap, luma is a Gaussian lowpass (response exp(-(f/luma_hz)^2), so
// edges barely ring) from which a windowed subcarrier-frequency cosine is
// subtracted, scaled so the response at the subcarrier is exactly zero: the
// colour carrier cannot leak into luma however wide luma is. luma_taps can
// lengthen the luma window, which narrows the notch.

struct KernelParams {
    float luma_hz = 2'000'000.0f;
    float i_hz = 1'300'000.0f;
    float q_hz = 600'000.0f;
    bool trap = false;
    int luma_taps = NUM_TAPS;    // window span +/- taps, in dots
    int chroma_taps = NUM_TAPS;
};

struct ContinuousKernel {
    static constexpr int SUB = 64;

    int k_min = -(NUM_TAPS + 1);
    int k_count = 2 * NUM_TAPS + 3;

    // [si][k - k_min][phase][Y,I,Q]
    std::vector<float> table;

    const float *at(int si, int k, int phase) const {
        return &table[(((size_t)si * k_count + (k - k_min)) * 4 + phase) * 3];
    }

    struct Window {
        int taps;
        std::vector<float> w;

        explicit Window(int t) : taps(t) {
            const int n = 2 * taps + 1;
            OEVector v = OEVector::chebyshevWindow(n, CHEBYSHEV_SIDELOBE_DB);
            w.resize(n);
            for (int i = 0; i < n; i++) w[i] = v.getValue(i);
            // chebyshevWindow halves the endpoints
            w[0] *= 2;
            w[n - 1] *= 2;
        }

        // linearly interpolated between taps, zero beyond +/- taps
        float operator()(float t) const {
            const int n = 2 * taps + 1;
            float pos = t + taps;
            if (pos < 0.0f || pos > (float)(n - 1)) return 0.0f;
            int i = (int)pos;
            if (i >= n - 1) return w[n - 1];
            float frac = pos - i;
            return w[i] + (w[i + 1] - w[i]) * frac;
        }
    };

    // integral of cos(2 pi f t) df from 0 to fc: an ideal lowpass impulse response
    static float lowpass(float t, float fc) {
        float x = 2.0f * (float)M_PI * t;
        return (x == 0.0f) ? fc : sinf(x * fc) / x;
    }

    // integral of exp(-(f/f0)^2) cos(2 pi f t) df from 0 to infinity
    static float gaussian(float t, float f0) {
        float a = (float)M_PI * f0 * t;
        return 0.5f * sqrtf((float)M_PI) * f0 * expf(-a * a);
    }

    void build(const KernelParams &kp, float subcarrier, float phase0) {
        constexpr int STEPS = 32;
        const Window wl(kp.luma_taps), wc(kp.chroma_taps);
        const int span = std::max(kp.luma_taps, kp.chroma_taps);
        k_min = -(span + 1);
        k_count = 2 * span + 3;
        table.assign((size_t)(SUB + 1) * k_count * 4 * 3, 0.0f);

        const float fy = kp.luma_hz / kFsc4, fi = kp.i_hz / kFsc4, fq = kp.q_hz / kFsc4;
        constexpr int NORM_STEPS = 256;
        float alpha = 0.0f;
        if (kp.trap) {
            double rh = 0, rc = 0;
            for (int s = 0; s < 2 * kp.luma_taps * NORM_STEPS; s++) {
                float t = -kp.luma_taps + (s + 0.5f) / NORM_STEPS;
                float c = cosf(2.0f * (float)M_PI * subcarrier * t);
                rh += wl(t) * gaussian(t, fy) * c;
                rc += wl(t) * c * c;
            }
            alpha = (float)(rh / rc);
        }
        auto luma = [&](float t) -> float {
            if (!kp.trap) return wl(t) * lowpass(t, fy);
            return wl(t) * (gaussian(t, fy) - alpha * cosf(2.0f * (float)M_PI * subcarrier * t));
        };
        auto chroma = [&](float t, float fc) -> float { return wc(t) * lowpass(t, fc); };

        // Normalize: luma DC gain 1, chroma 2 (as generate_filters does).
        double sy = 0, si_ = 0, sq = 0;
        for (int s = 0; s < 2 * span * NORM_STEPS; s++) {
            float t = -span + (s + 0.5f) / NORM_STEPS;
            sy += luma(t);
            si_ += chroma(t, fi);
            sq += chroma(t, fq);
        }
        const float gy = (float)(NORM_STEPS / sy);
        const float gi = (float)(2.0 * NORM_STEPS / si_);
        const float gq = (float)(2.0 * NORM_STEPS / sq);

        for (int s = 0; s <= SUB; s++) {
            float frac = (float)s / SUB;
            for (int k = k_min; k < k_min + k_count; k++) {
                // dot centre at base + k, output at base + frac
                float d = frac - k;
                for (int p = 0; p < 4; p++) {
                    double y = 0, i = 0, q = 0;
                    for (int st = 0; st < STEPS; st++) {
                        float v = -0.5f + (st + 0.5f) / STEPS;  // position within the dot
                        float t = d - v;                         // output minus this slice
                        float ph = 2.0f * (float)M_PI * (subcarrier * (p + v) + phase0);
                        y += luma(t) * gy;
                        i += chroma(t, fi) * gi * sinf(ph);
                        q += chroma(t, fq) * gq * cosf(ph);
                    }
                    float *o = &table[(((size_t)s * k_count + (k - k_min)) * 4 + p) * 3];
                    o[0] = (float)(y / STEPS);
                    o[1] = (float)(i / STEPS);
                    o[2] = (float)(q / STEPS);
                }
            }
        }
    }
};

} // namespace ntsc_proto
