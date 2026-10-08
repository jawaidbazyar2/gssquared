/**
 * ntsc1120 - render a static HGR / DHGR image through several NTSC decoders
 * so the effect of decoding at 1120 samples per line (8fsc) can be compared
 * against the production 560-sample (4fsc) lookup-table decoder.
 *
 *   ntsc1120 [--hgr|--dhgr] [--crop x,y,w,h] [--zoom N] <file> <outdir>
 *
 * Crop is in Apple II dots / scanlines (560x192 space). Zoom is the dot
 * magnification of the crop images (even, default 8). --luma / --i / --q set
 * the filter bandwidths (MHz) of the direct decoders (s1120_*, p560*); the
 * legacy560 / held560 variants always use the production LUT defaults.
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "display/ntsc.hpp"
#include "display/filters.hpp"
#include "display/OEVector.h"
#include "ntsc_proto.hpp"

using namespace ntsc_proto;

static constexpr int kSamples = 1120;

// Defaults match init_display_configuration() in src/display/filters.cpp
// (Q = chroma bandwidth, I = chroma + NTSC I/Q delta). The production LUT
// variants always use these defaults; the direct decoders use --luma/--i/--q.
static float g_luma_bw = 2'000'000.0f;
static float g_i_bw = 1'300'000.0f;
static float g_q_bw = 600'000.0f;

/** RGB image on the 1120-sample grid, values 0..255. */
struct Image {
    std::string name;
    std::vector<float> px = std::vector<float>(kLines * kSamples * 3, 0.0f);
    float *at(int y, int s) { return &px[(y * kSamples + s) * 3]; }
    const float *at(int y, int s) const { return &px[(y * kSamples + s) * 3]; }
};

static void upscale_nearest(const Line560 &in, Image &img, int y) {
    for (int s = 0; s < kSamples; s++) {
        float *o = img.at(y, s);
        for (int c = 0; c < 3; c++) o[c] = in[s / 2][c];
    }
}

static void upscale_lanczos(const Line560 &in, Image &img, int y) {
    for (int s = 0; s < kSamples; s++) {
        // dot x is centred between samples 2x and 2x+1
        float u = (s - 0.5f) / 2.0f;
        int base = (int)std::floor(u);
        float acc[3] = {0, 0, 0};
        float wsum = 0;
        for (int n = base - 2; n <= base + 3; n++) {
            float w = lanczos3(u - n);
            int idx = std::clamp(n, 0, kDots - 1);
            for (int c = 0; c < 3; c++) acc[c] += w * in[idx][c];
            wsum += w;
        }
        float *o = img.at(y, s);
        for (int c = 0; c < 3; c++) o[c] = std::clamp(acc[c] / wsum, 0.0f, 255.0f);
    }
}

// ---------------------------------------------------------------------------
// Direct (non-LUT) decode: each dot is `spd` samples (1 = 4fsc / 560,
// 2 = 8fsc / 1120) and the YIQ FIR runs at every sample. Filter design
// follows generate_filters() in filters.cpp at the given sample rate.

struct Fir {
    int half = 0;
    std::vector<float> y, i, q;
};

static Fir make_fir(int ntaps, int spd) {
    const float rate = 4.0f * spd * kFsc;
    OEVector w = OEVector::chebyshevWindow(ntaps, CHEBYSHEV_SIDELOBE_DB);
    w = w.normalize();
    OEVector wy = w * OEVector::lanczosWindow(ntaps, g_luma_bw / rate);
    wy = wy.normalize();
    OEVector wu = w * OEVector::lanczosWindow(ntaps, g_i_bw / rate);
    wu = wu.normalize() * 2;
    OEVector wv = w * OEVector::lanczosWindow(ntaps, g_q_bw / rate);
    wv = wv.normalize() * 2;

    Fir f;
    f.half = ntaps / 2;
    for (int k = 0; k < ntaps; k++) {
        f.y.push_back(wy.getValue(k));
        f.i.push_back(wu.getValue(k));
        f.q.push_back(wv.getValue(k));
    }
    return f;
}

/** Decode one line into `out` (n = 560 * spd RGB triples). */
static void decode_direct(const uint8_t *dots, uint8_t phase_offset, const Fir &fir, int spd, float (*out)[3]) {
    const int n = kDots * spd;
    std::vector<float> sig(n), sinp(n), cosp(n);
    const float phase0 = config.phaseInfo[0];
    for (int s = 0; s < n; s++) {
        sig[s] = dots[s / spd] ? 1.0f : 0.0f;
        // The samples of dot x straddle the 560 decoder's phase for that dot,
        // so every variant shares the same hue reference.
        float pos = spd * phase_offset + s - 0.5f * (spd - 1);
        float ph = 2.0f * (float)M_PI * (pos / (4.0f * spd) + phase0);
        sinp[s] = sinf(ph);
        cosp[s] = cosf(ph);
    }
    const Matrix3x3 &m = config.decoderMatrix;
    for (int s = 0; s < n; s++) {
        float Y = 0, I = 0, Q = 0;
        for (int k = -fir.half; k <= fir.half; k++) {
            int t = s + k;
            if (t < 0 || t >= n || sig[t] == 0.0f) continue;
            Y += fir.y[k + fir.half];
            I += fir.i[k + fir.half] * sinp[t];
            Q += fir.q[k + fir.half] * cosp[t];
        }
        out[s][0] = (float)std::clamp((int)((m[0] * Y + m[1] * I + m[2] * Q) * 255), 0, 255);
        out[s][1] = (float)std::clamp((int)((m[3] * Y + m[4] * I + m[5] * Q) * 255), 0, 255);
        out[s][2] = (float)std::clamp((int)((m[6] * Y + m[7] * I + m[8] * Q) * 255), 0, 255);
    }
}

// ---------------------------------------------------------------------------
// Output

struct Raster {
    int w = 0, h = 0;
    std::vector<uint8_t> rgb;
};

/** Crop (in dots / lines) and scale: sx output pixels per sample, sy per line. */
static Raster rasterize(const Image &img, int cx, int cy, int cw, int ch, int sx, int sy) {
    Raster r;
    r.w = cw * 2 * sx;
    r.h = ch * sy;
    r.rgb.resize((size_t)r.w * r.h * 3);
    for (int Y = 0; Y < r.h; Y++) {
        int line = cy + Y / sy;
        for (int X = 0; X < r.w; X++) {
            int s = cx * 2 + X / sx;
            const float *p = img.at(line, s);
            uint8_t *o = &r.rgb[((size_t)Y * r.w + X) * 3];
            for (int c = 0; c < 3; c++) o[c] = (uint8_t)std::lround(p[c]);
        }
    }
    return r;
}

static bool save_png(const Raster &r, const std::string &path) {
    SDL_Surface *surf = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_RGB24);
    if (!surf) {
        fprintf(stderr, "SDL_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }
    for (int y = 0; y < r.h; y++) {
        memcpy((uint8_t *)surf->pixels + (size_t)y * surf->pitch, &r.rgb[(size_t)y * r.w * 3], (size_t)r.w * 3);
    }
    bool ok = IMG_SavePNG(surf, path.c_str());
    if (!ok) fprintf(stderr, "IMG_SavePNG(%s) failed: %s\n", path.c_str(), SDL_GetError());
    SDL_DestroySurface(surf);
    return ok;
}

static Raster stack(const std::vector<Raster> &parts, int gap) {
    Raster r;
    r.w = parts.empty() ? 0 : parts[0].w;
    for (const Raster &p : parts) r.h += p.h;
    r.h += gap * (int)(parts.size() > 0 ? parts.size() - 1 : 0);
    r.rgb.assign((size_t)r.w * r.h * 3, 0x60);
    int y0 = 0;
    for (const Raster &p : parts) {
        memcpy(&r.rgb[(size_t)y0 * r.w * 3], p.rgb.data(), p.rgb.size());
        y0 += p.h + gap;
    }
    return r;
}

static void diff_stats(const Image &a, const Image &ref, double &rms, double &maxd) {
    double sum = 0;
    maxd = 0;
    for (size_t i = 0; i < a.px.size(); i++) {
        double d = (double)a.px[i] - ref.px[i];
        sum += d * d;
        maxd = std::max(maxd, std::fabs(d));
    }
    rms = std::sqrt(sum / a.px.size());
}

// ---------------------------------------------------------------------------

static void usage() {
    fprintf(stderr, "usage: ntsc1120 [--hgr|--dhgr] [--crop x,y,w,h] [--zoom N] [--luma MHz] [--i MHz] [--q MHz] <file> <outdir>\n");
}

int main(int argc, char **argv) {
    int force_mode = 0; // 0 auto, 1 hgr, 2 dhgr
    int cx = 210, cy = 72, cw = 140, ch = 48;
    int zoom = 8;
    std::vector<std::string> pos;
    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg == "--hgr") force_mode = 1;
        else if (arg == "--dhgr") force_mode = 2;
        else if (arg == "--crop" && a + 1 < argc) {
            if (sscanf(argv[++a], "%d,%d,%d,%d", &cx, &cy, &cw, &ch) != 4) { usage(); return 1; }
        } else if (arg == "--zoom" && a + 1 < argc) {
            zoom = atoi(argv[++a]);
        } else if (arg == "--luma" && a + 1 < argc) {
            g_luma_bw = (float)atof(argv[++a]) * 1e6f;
        } else if (arg == "--i" && a + 1 < argc) {
            g_i_bw = (float)atof(argv[++a]) * 1e6f;
        } else if (arg == "--q" && a + 1 < argc) {
            g_q_bw = (float)atof(argv[++a]) * 1e6f;
        } else if (arg.rfind("--", 0) == 0) { usage(); return 1; }
        else pos.push_back(arg);
    }
    if (pos.size() != 2) { usage(); return 1; }
    if (zoom < 2 || (zoom & 1)) { fprintf(stderr, "zoom must be even and >= 2\n"); return 1; }
    cx = std::clamp(cx, 0, kDots - 1);
    cy = std::clamp(cy, 0, kLines - 1);
    cw = std::clamp(cw, 1, kDots - cx);
    ch = std::clamp(ch, 1, kLines - cy);

    const std::string &path = pos[0];
    const std::string &outdir = pos[1];

    Bitstream *bs = new Bitstream();
    bool dhgr = false;
    size_t n = 0;
    if (!load_page(path, force_mode, *bs, dhgr, n)) { fprintf(stderr, "cannot open %s\n", path.c_str()); return 1; }

    std::string name = base_name(path);
    printf("%s: %s, %zu bytes; direct decoders: luma %.2f MHz, I %.2f MHz, Q %.2f MHz\n", name.c_str(),
           dhgr ? "DHGR" : "HGR", n, g_luma_bw / 1e6f, g_i_bw / 1e6f, g_q_bw / 1e6f);

    setupConfig();
    generate_filters(NUM_TAPS);

    std::vector<Image> imgs(7);
    imgs[0].name = "legacy560";
    imgs[1].name = "held560";
    imgs[2].name = "s1120_29";
    imgs[3].name = "s1120_15";
    imgs[4].name = "held560_lanczos";
    imgs[5].name = "p560";
    imgs[6].name = "p560_lanczos";

    static Line560 line;

    config.heldDots = false;
    init_hgr_LUT();
    for (int y = 0; y < kLines; y++) {
        decode_lut560(g_hgr_LUT, bs->dots[y], bs->phase_offset, line);
        upscale_nearest(line, imgs[0], y);
    }

    config.heldDots = true;
    init_hgr_LUT();
    for (int y = 0; y < kLines; y++) {
        decode_lut560(g_hgr_LUT, bs->dots[y], bs->phase_offset, line);
        upscale_nearest(line, imgs[1], y);
        upscale_lanczos(line, imgs[4], y);
    }

    Fir fir29 = make_fir(4 * NUM_TAPS + 1, 2);
    Fir fir15 = make_fir(2 * NUM_TAPS + 1, 2);
    Fir fir560 = make_fir(2 * NUM_TAPS + 1, 1);
    for (int y = 0; y < kLines; y++) {
        decode_direct(bs->dots[y], bs->phase_offset, fir29, 2, reinterpret_cast<float (*)[3]>(imgs[2].at(y, 0)));
        decode_direct(bs->dots[y], bs->phase_offset, fir15, 2, reinterpret_cast<float (*)[3]>(imgs[3].at(y, 0)));
        decode_direct(bs->dots[y], bs->phase_offset, fir560, 1, line);
        upscale_nearest(line, imgs[5], y);
        upscale_lanczos(line, imgs[6], y);
    }

    SDL_CreateDirectory(outdir.c_str());

    printf("\nDifference vs s1120_29 (8-bit levels):\n");
    std::vector<Raster> zooms;
    for (const Image &img : imgs) {
        double rms, maxd;
        diff_stats(img, imgs[2], rms, maxd);
        printf("  %-16s RMS %6.2f  max %6.1f\n", img.name.c_str(), rms, maxd);

        save_png(rasterize(img, 0, 0, kDots, kLines, 1, 4), outdir + "/" + name + "_" + img.name + ".png");
        Raster z = rasterize(img, cx, cy, cw, ch, zoom / 2, zoom * 2);
        save_png(z, outdir + "/" + name + "_" + img.name + "_zoom.png");
        zooms.push_back(std::move(z));
    }
    save_png(stack(zooms, 8), outdir + "/" + name + "_sheet.png");

    printf("\nSheet order (top to bottom):");
    for (const Image &img : imgs) printf(" %s", img.name.c_str());
    printf("\nCrop: x=%d y=%d w=%d h=%d dots/lines, zoom %dx\n", cx, cy, cw, ch, zoom);

    delete bs;
    return 0;
}
