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
 */
static void generate_held_weights(OEVector &w, int num_taps,
                                  float yBandwidth, float uBandwidth, float vBandwidth) {
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

    // Integrate the plain kernels first so they can be normalized the same way
    // generate_filters normalizes the discrete taps (luma DC gain 1, chroma 2).
    double sum_y = 0, sum_u = 0, sum_v = 0;
    for (int k = -num_taps; k <= num_taps; k++) {
        for (int s = 0; s < SUBSTEPS; s++) {
            float t = k - 0.5f + (s + 0.5f) / SUBSTEPS;
            float wt = window_at(t);
            sum_y += wt * sinc_at(t, yBandwidth);
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
                y += wt * sinc_at(t, yBandwidth) * gain_y;
                i += wt * sinc_at(t, uBandwidth) * gain_u * sinf(phase);
                q += wt * sinc_at(t, vBandwidth) * gain_v * cosf(phase);
            }
            config.heldWeights[k + num_taps][ph][0] = (float)(y / SUBSTEPS);
            config.heldWeights[k + num_taps][ph][1] = (float)(i / SUBSTEPS);
            config.heldWeights[k + num_taps][ph][2] = (float)(q / SUBSTEPS);
        }
    }
}

void generate_filters(int num_taps) {
    init_display_configuration();

    int total_samples = num_taps * 2 + 1;

    // Filters
    OEVector w = OEVector::chebyshevWindow(total_samples, CHEBYSHEV_SIDELOBE_DB); // orig 50.
    w = w.normalize();
    printf("Chebyshev window\n");
    w.print();
    
    OEVector wy, wu, wv;
    
    float bandwidth = displayConfiguration.videoBandwidth / displayConfiguration.imageSampleRate;
    

        float yBandwidth = displayConfiguration.videoLumaBandwidth / displayConfiguration.imageSampleRate;
        float uBandwidth = displayConfiguration.videoChromaBandwidth / displayConfiguration.imageSampleRate;
        float vBandwidth = uBandwidth;
        
printf("yBandwidth: %f\n", yBandwidth);
printf("uBandwidth: %f\n", uBandwidth);
printf("vBandwidth: %f\n", vBandwidth);

       /*  if (displayConfiguration.videoDecoder == CANVAS_YIQ) */
            uBandwidth = uBandwidth + NTSC_IQ_DELTA / displayConfiguration.imageSampleRate;
        
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

    generate_held_weights(w, num_taps, yBandwidth, uBandwidth, vBandwidth);
}