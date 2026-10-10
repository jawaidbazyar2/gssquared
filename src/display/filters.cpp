#include <cmath>
#include <vector>

#include "OEVector.h"

#include "ntsc.hpp"

typedef enum
{
    CANVAS_RGB,
    CANVAS_MONOCHROME,
    CANVAS_YUV,
    CANVAS_YIQ,
    CANVAS_CXA2025AS,
} CanvasDecoder;

#define NTSC_FSC        (315.0F / 88.0F * 1E6F)
#define NTSC_4FSC       (4 * NTSC_FSC)

#define NTSC_I_CUTOFF               1300000
#define NTSC_Q_CUTOFF               600000
#define NTSC_IQ_DELTA               (NTSC_I_CUTOFF - NTSC_Q_CUTOFF)

// Held-dot weights with the chroma trap: Gaussian luma 1/e frequency, I and Q cutoffs.
#define TRAP_LUMA_BANDWIDTH         3'500'000.0f
#define TRAP_I_BANDWIDTH            2'000'000.0f
#define TRAP_Q_BANDWIDTH            1'300'000.0f

struct DisplayConfiguration {
    float videoBandwidth;
    float videoLumaBandwidth;
    float videoChromaBandwidth;
    float videoDecoder;
    float videoWhiteOnly;
    float imageSampleRate;
    float imageSubcarrier;
};

DisplayConfiguration displayConfiguration;

extern ntsc_config config ;

void init_display_configuration() {
    displayConfiguration.videoBandwidth = 4'500'000.0f;
    displayConfiguration.videoLumaBandwidth = 2'000'000.0f;
    displayConfiguration.videoChromaBandwidth = 600'000.0f;
    displayConfiguration.videoDecoder = CANVAS_YIQ;
    displayConfiguration.videoWhiteOnly = 0;
    displayConfiguration.imageSampleRate = NTSC_4FSC;
    displayConfiguration.imageSubcarrier = NTSC_FSC;
}

/**
 * Held-dot filter weights.
 *
 * The Apple II holds each dot's level for a full 4fsc sample period, so the
 * analog decoder sees a square pulse, not an impulse. A dot's contribution to
 * the output is the demodulated kernel integrated across that pulse. This
 * attenuates chroma by a frequency-dependent amount (about 10% at fsc), which
 * the point-sampled model cannot represent with a constant saturation factor.
 *
 * The Chebyshev window only exists at tap positions; it is linearly
 * interpolated between them. Its endpoint taps were halved by chebyshevWindow
 * (periodic-window convention), so they are restored here, and the window is
 * zero beyond +/- num_taps, which leaves the outermost dots half covered.
 *
 * With trap set, luma is a Gaussian lowpass (response exp(-(f/yBandwidth)^2),
 * which barely rings) minus a windowed subcarrier cosine scaled so the response
 * at the subcarrier is exactly zero, like a monitor's chroma trap. The carrier
 * then cannot stripe solid colours however wide luma is.
 */
static void generate_held_weights(OEVector &w, int num_taps,
                                  float yBandwidth, float uBandwidth, float vBandwidth, bool trap) {
    constexpr int SUBSTEPS = 64;
    const int total_samples = num_taps * 2 + 1;

    std::vector<float> win(total_samples);
    for (int i = 0; i < total_samples; i++) win[i] = w.getValue(i);
    win[0] *= 2;
    win[total_samples - 1] *= 2;

    auto window_at = [&](float t) -> float {
        float pos = t + num_taps;
        if (pos < 0.0f || pos > (float)(total_samples - 1)) return 0.0f;
        int i = (int)pos;
        if (i >= total_samples - 1) return win[total_samples - 1];
        float frac = pos - i;
        return win[i] + (win[i + 1] - win[i]) * frac;
    };
    auto sinc_at = [](float t, float fc) -> float {
        float x = 2.0f * (float)M_PI * fc * t;
        return (x == 0.0f) ? 1.0f : sinf(x) / x;
    };
    auto carrier_at = [](float t) -> float {
        return cosf(2.0f * (float)M_PI * config.subcarrier * t);
    };

    float alpha = 0.0f;
    if (trap) {
        double rh = 0, rc = 0;
        for (int k = -num_taps; k <= num_taps; k++) {
            for (int s = 0; s < SUBSTEPS; s++) {
                float t = k - 0.5f + (s + 0.5f) / SUBSTEPS;
                float wt = window_at(t);
                float a = (float)M_PI * yBandwidth * t;
                rh += wt * expf(-a * a) * carrier_at(t);
                rc += wt * carrier_at(t) * carrier_at(t);
            }
        }
        alpha = (float)(rh / rc);
    }
    auto luma_at = [&](float t) -> float {
        if (!trap) return sinc_at(t, yBandwidth);
        float a = (float)M_PI * yBandwidth * t;
        return expf(-a * a) - alpha * carrier_at(t);
    };

    // Integrate the plain kernels first so they can be normalized the same way
    // generate_filters normalizes the discrete taps (luma DC gain 1, chroma 2).
    double sum_y = 0, sum_u = 0, sum_v = 0;
    for (int k = -num_taps; k <= num_taps; k++) {
        for (int s = 0; s < SUBSTEPS; s++) {
            float t = k - 0.5f + (s + 0.5f) / SUBSTEPS;
            float wt = window_at(t);
            sum_y += wt * luma_at(t);
            sum_u += wt * sinc_at(t, uBandwidth);
            sum_v += wt * sinc_at(t, vBandwidth);
        }
    }
    const float gain_y = (float)(SUBSTEPS / sum_y);
    const float gain_u = (float)(2.0 * SUBSTEPS / sum_u);
    const float gain_v = (float)(2.0 * SUBSTEPS / sum_v);

    for (int k = -num_taps; k <= num_taps; k++) {
        for (int ph = 0; ph < 4; ph++) {
            double y = 0, i = 0, q = 0;
            for (int s = 0; s < SUBSTEPS; s++) {
                float u = -0.5f + (s + 0.5f) / SUBSTEPS;
                float t = k + u;
                float wt = window_at(t);
                float phase = 2.0f * (float)M_PI * (config.subcarrier * (ph + u) + config.phaseInfo[0]);
                y += wt * luma_at(t) * gain_y;
                i += wt * sinc_at(t, uBandwidth) * gain_u * sinf(phase);
                q += wt * sinc_at(t, vBandwidth) * gain_v * cosf(phase);
            }
            config.heldWeights[k + num_taps][ph][0] = (float)(y / SUBSTEPS);
            config.heldWeights[k + num_taps][ph][1] = (float)(i / SUBSTEPS);
            config.heldWeights[k + num_taps][ph][2] = (float)(q / SUBSTEPS);
        }
    }
}

// Classic point-sample filter (Chebyshev window x Lanczos) for a given sample rate.
static void design_point_filters(int num_taps, float sample_rate, OEVector &w,
                                 OEVector &wy, OEVector &wu, OEVector &wv,
                                 float &yBandwidth, float &uBandwidth, float &vBandwidth)
{
    int total_samples = num_taps * 2 + 1;

    w = OEVector::chebyshevWindow(total_samples, CHEBYSHEV_SIDELOBE_DB); // orig 50.
    w = w.normalize();

    float bandwidth = displayConfiguration.videoBandwidth / sample_rate;
    yBandwidth = displayConfiguration.videoLumaBandwidth / sample_rate;
    uBandwidth = displayConfiguration.videoChromaBandwidth / sample_rate;
    vBandwidth = uBandwidth;

    /* if (displayConfiguration.videoDecoder == CANVAS_YIQ) */
        uBandwidth = uBandwidth + NTSC_IQ_DELTA / sample_rate;

    // Switch to video bandwidth when no subcarrier
    if ((displayConfiguration.imageSubcarrier == 0.0) ||
        (displayConfiguration.videoWhiteOnly))
    {
        yBandwidth = bandwidth;
        uBandwidth = bandwidth;
        vBandwidth = bandwidth;
    }

    wy = w * OEVector::lanczosWindow(total_samples, yBandwidth);
    wy = wy.normalize();

    wu = w * OEVector::lanczosWindow(total_samples, uBandwidth);
    wu = wu.normalize() * 2;

    wv = w * OEVector::lanczosWindow(total_samples, vBandwidth);
    wv = wv.normalize() * 2;
}

// Point-sample version of the held-dot trap weights, taps 0..num_taps: Gaussian
// luma (1/e at luma_bandwidth Hz) with an exact null at the subcarrier, and
// windowed-sinc I / Q (cutoffs i_bandwidth, q_bandwidth Hz).
// rate_scale multiplies the 4fsc sample rate (16/14 for SHR lines), which moves
// the subcarrier to config.subcarrier / rate_scale cycles per sample.
void generate_trap_point_filters(float rate_scale, int num_taps, float luma_bandwidth,
                                 float i_bandwidth, float q_bandwidth, float out[][3]) {
    init_display_configuration();
    const int total_samples = num_taps * 2 + 1;
    const float rate = displayConfiguration.imageSampleRate * rate_scale;
    const float yb = luma_bandwidth / rate;
    const float ib = i_bandwidth / rate;
    const float qb = q_bandwidth / rate;
    const float fsc = config.subcarrier / rate_scale;

    OEVector w = OEVector::chebyshevWindow(total_samples, CHEBYSHEV_SIDELOBE_DB).normalize();
    auto win = [&](int k) { return w.getValue(k + num_taps); };
    auto gauss = [&](int k) { float a = (float)M_PI * yb * k; return expf(-a * a); };
    auto carrier = [&](int k) { return cosf(2.0f * (float)M_PI * fsc * k); };
    auto sinc = [](int k, float fc) { float x = 2.0f * (float)M_PI * fc * k; return k == 0 ? 1.0f : sinf(x) / x; };

    auto carrier2 = [&](int k) { return cosf(4.0f * (float)M_PI * fsc * k); };

    double rh = 0, rc = 0;
    for (int k = -num_taps; k <= num_taps; k++) {
        rh += win(k) * gauss(k) * carrier(k);
        rc += win(k) * carrier(k) * carrier(k);
    }
    const float alpha = (float)(rh / rc);

    // Chroma: subtract a * carrier + b * carrier2 so the response is zero at fsc
    // (luma leaking into I / Q tints solid colors) and at 2 fsc (demodulation image).
    auto chroma_nulls = [&](float fc, float &a, float &b) {
        double m11 = 0, m12 = 0, m21 = 0, m22 = 0, r1 = 0, r2 = 0;
        for (int k = -num_taps; k <= num_taps; k++) {
            double wk = win(k), c1 = carrier(k), c2 = carrier2(k), s = sinc(k, fc);
            m11 += wk * c1 * c1; m12 += wk * c2 * c1; r1 += wk * s * c1;
            m21 += wk * c1 * c2; m22 += wk * c2 * c2; r2 += wk * s * c2;
        }
        double det = m11 * m22 - m12 * m21;
        a = (float)((r1 * m22 - m12 * r2) / det);
        b = (float)((m11 * r2 - m21 * r1) / det);
    };
    float ia, ib2, qa, qb2;
    chroma_nulls(ib, ia, ib2);
    chroma_nulls(qb, qa, qb2);
    auto ifilt = [&](int k) { return win(k) * (sinc(k, ib) - ia * carrier(k) - ib2 * carrier2(k)); };
    auto qfilt = [&](int k) { return win(k) * (sinc(k, qb) - qa * carrier(k) - qb2 * carrier2(k)); };

    double sy = 0, si = 0, sq = 0;
    for (int k = -num_taps; k <= num_taps; k++) {
        sy += win(k) * (gauss(k) - alpha * carrier(k));
        si += ifilt(k);
        sq += qfilt(k);
    }
    for (int k = 0; k <= num_taps; k++) {
        out[k][0] = (float)(win(k) * (gauss(k) - alpha * carrier(k)) / sy);
        out[k][1] = (float)(2.0 * ifilt(k) / si);
        out[k][2] = (float)(2.0 * qfilt(k) / sq);
    }
}

void generate_filters(int num_taps) {
    init_display_configuration();

    OEVector w, wy, wu, wv;
    float yBandwidth, uBandwidth, vBandwidth;
    design_point_filters(num_taps, displayConfiguration.imageSampleRate, w, wy, wu, wv,
                         yBandwidth, uBandwidth, vBandwidth);
    printf("Chebyshev window\n");
    w.print();

printf("yBandwidth: %f\n", yBandwidth);
printf("uBandwidth: %f\n", uBandwidth);
printf("vBandwidth: %f\n", vBandwidth);

    printf("wy\n");
    wy.print();
    printf("wu\n");
    wu.print();
    printf("wv\n");
    wv.print();

    for (int i = 0; i <= num_taps; i++) {
        int ind = i + num_taps;
        printf("wy[%d]: %f ", ind, wy.getValue(ind));
        config.filterCoefficients[i][0] = wy.getValue(ind);
        printf("wu[%d]: %f ", ind, wu.getValue(ind));
        config.filterCoefficients[i][1] = wu.getValue(ind);
        printf("wv[%d]: %f\n", ind, wv.getValue(ind));
        config.filterCoefficients[i][2] = wv.getValue(ind);
    }

    if (config.lumaTrap) {
        generate_held_weights(w, num_taps, TRAP_LUMA_BANDWIDTH / displayConfiguration.imageSampleRate,
                              TRAP_I_BANDWIDTH / displayConfiguration.imageSampleRate,
                              TRAP_Q_BANDWIDTH / displayConfiguration.imageSampleRate, true);
    } else {
        generate_held_weights(w, num_taps, yBandwidth, uBandwidth, vBandwidth, false);
    }
}
