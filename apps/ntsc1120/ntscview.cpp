/**
 * ntscview - static HGR / DHGR viewer that evaluates the NTSC decoder directly
 * at every window pixel column, so no scaling happens after decoding.
 *
 *   ntscview [--hgr|--dhgr] [--luma MHz] [--i MHz] [--q MHz]
 *            [--trap] [--luma-taps N] [--mode 1|2|3]
 *            [--size WxH --screenshot out.png] <file> [file...]
 *
 * Keys: 1 exact (continuous held-dot kernel at window resolution)
 *       2 the same kernel at the 560 dot centres + Lanczos-3 to window width
 *       3 legacy560 LUT + nearest
 *       Left/Right cycle files, S save PNG to /tmp, Esc quit.
 *
 * The image keeps the 1120:768 (560x192 at 2x4) aspect, letterboxed in the
 * window. Vertically each output row shows the nearest of the 192 scanlines.
 * --size/--screenshot renders one frame at exactly WxH pixels and exits.
 * The filter options apply to modes 1 and 2; mode 3 always uses the production
 * default bandwidths. --trap switches luma to a Gaussian lowpass (--luma is
 * its 1/e frequency) with an exact null at the subcarrier, the way a monitor's
 * chroma trap removes the carrier. --luma-taps sets the luma window span
 * (default 7, the production span); longer narrows the notch.
 */

#include <algorithm>
#include <chrono>
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
#include "ntsc_proto.hpp"

using namespace ntsc_proto;

static constexpr int kLutEntries = 1 << ((NUM_TAPS * 2) + 1);

enum Mode { MODE_EXACT = 1, MODE_LANCZOS = 2, MODE_NEAREST = 3 };

static const char *mode_name(int m) {
    switch (m) {
        case MODE_EXACT: return "exact";
        case MODE_LANCZOS: return "dot560_lanczos";
        default: return "legacy560_nearest";
    }
}

struct Source {
    std::string path;
    std::string name;
    Bitstream *bs = nullptr;
};

struct Viewer {
    std::vector<Source> sources;
    int cur = 0;
    int mode = MODE_EXACT;
    ContinuousKernel kernel;
    std::vector<RGBA_t> lut_legacy = std::vector<RGBA_t>(4 * kLutEntries);

    // last rendered frame, RGB24
    int fw = 0, fh = 0;
    int cx = 0, cy = 0, cw = 0, ch = 0;
    std::vector<uint8_t> frame;
    double last_ms = 0;
};

static inline uint8_t to_u8(float v) {
    return (uint8_t)std::clamp((int)(v * 255), 0, 255);
}

/** Sum the continuous kernel for every set dot near output position base + si/SUB. */
static void eval_kernel(const ContinuousKernel &kernel, const Bitstream &bs, int line, int b, int si, float rgb[3]) {
    const uint8_t *dots = bs.dots[line];
    const Matrix3x3 &m = config.decoderMatrix;
    const int po = bs.phase_offset;
    float Y = 0, I = 0, Q = 0;
    for (int k = kernel.k_min; k < kernel.k_min + kernel.k_count; k++) {
        int n = b + k;
        if (n < 0 || n >= kDots || !dots[n]) continue;
        const float *w = kernel.at(si, k, (po + n) & 3);
        Y += w[0];
        I += w[1];
        Q += w[2];
    }
    rgb[0] = m[0] * Y + m[1] * I + m[2] * Q;
    rgb[1] = m[3] * Y + m[4] * I + m[5] * Q;
    rgb[2] = m[6] * Y + m[7] * I + m[8] * Q;
}

/** Mode 1: evaluate the kernel at every output column. */
static void render_line_exact(const Viewer &v, const Bitstream &bs, int line, int cw,
                              const std::vector<int> &base, const std::vector<int> &sub, uint8_t *out) {
    for (int X = 0; X < cw; X++) {
        float rgb[3];
        eval_kernel(v.kernel, bs, line, base[X], sub[X], rgb);
        for (int c = 0; c < 3; c++) out[X * 3 + c] = to_u8(rgb[c]);
    }
}

/** Mode 2 first stage: evaluate the kernel at the 560 dot centres, quantized like the LUT. */
static void decode_dots560(const Viewer &v, const Bitstream &bs, int line, Line560 &out) {
    for (int x = 0; x < kDots; x++) {
        float rgb[3];
        eval_kernel(v.kernel, bs, line, x, 0, rgb);
        for (int c = 0; c < 3; c++) out[x][c] = to_u8(rgb[c]);
    }
}

static void render_line_lanczos(const Line560 &in, int cw, const std::vector<float> &u, uint8_t *out) {
    for (int X = 0; X < cw; X++) {
        int b = (int)std::floor(u[X]);
        float acc[3] = {0, 0, 0};
        float wsum = 0;
        for (int n = b - 2; n <= b + 3; n++) {
            float w = lanczos3(u[X] - n);
            int idx = std::clamp(n, 0, kDots - 1);
            for (int c = 0; c < 3; c++) acc[c] += w * in[idx][c];
            wsum += w;
        }
        for (int c = 0; c < 3; c++) out[X * 3 + c] = (uint8_t)std::clamp(acc[c] / wsum, 0.0f, 255.0f);
    }
}

static void render_line_nearest(const Line560 &in, int cw, const std::vector<float> &u, uint8_t *out) {
    for (int X = 0; X < cw; X++) {
        int idx = std::clamp((int)std::floor(u[X] + 0.5f), 0, kDots - 1);
        for (int c = 0; c < 3; c++) out[X * 3 + c] = (uint8_t)in[idx][c];
    }
}

/** Render the current source/mode into v.frame at exactly w x h pixels. */
static void render_frame(Viewer &v, int w, int h) {
    auto t0 = std::chrono::steady_clock::now();
    v.fw = std::max(w, 1);
    v.fh = std::max(h, 1);
    v.frame.assign((size_t)v.fw * v.fh * 3, 0);

    // content rect with 1120:768 aspect, letterboxed
    constexpr double aspect = 1120.0 / 768.0;
    if ((double)v.fw / v.fh > aspect) {
        v.ch = v.fh;
        v.cw = std::max(1, (int)std::lround(v.fh * aspect));
    } else {
        v.cw = v.fw;
        v.ch = std::max(1, (int)std::lround(v.fw / aspect));
    }
    v.cx = (v.fw - v.cw) / 2;
    v.cy = (v.fh - v.ch) / 2;

    const int cw = v.cw, ch = v.ch;
    std::vector<float> u(cw);
    std::vector<int> base(cw), sub(cw);
    for (int X = 0; X < cw; X++) {
        u[X] = (float)((X + 0.5) * kDots / cw - 0.5);
        int b = (int)std::floor(u[X]);
        base[X] = b;
        sub[X] = (int)std::lround((u[X] - b) * ContinuousKernel::SUB);
    }

    std::vector<int> row_line(ch);
    std::vector<bool> needed(kLines, false);
    for (int Y = 0; Y < ch; Y++) {
        row_line[Y] = std::min(kLines - 1, (int)std::floor((Y + 0.5) * kLines / ch));
        needed[row_line[Y]] = true;
    }

    const Bitstream &bs = *v.sources[v.cur].bs;
    std::vector<uint8_t> lines((size_t)kLines * cw * 3);
    static Line560 l560;
    for (int line = 0; line < kLines; line++) {
        if (!needed[line]) continue;
        uint8_t *out = &lines[(size_t)line * cw * 3];
        switch (v.mode) {
            case MODE_EXACT:
                render_line_exact(v, bs, line, cw, base, sub, out);
                break;
            case MODE_LANCZOS:
                decode_dots560(v, bs, line, l560);
                render_line_lanczos(l560, cw, u, out);
                break;
            default:
                decode_lut560(reinterpret_cast<const RGBA_t (*)[kLutEntries]>(v.lut_legacy.data()), bs.dots[line],
                              bs.phase_offset, l560);
                render_line_nearest(l560, cw, u, out);
                break;
        }
    }
    for (int Y = 0; Y < ch; Y++) {
        memcpy(&v.frame[((size_t)(v.cy + Y) * v.fw + v.cx) * 3], &lines[(size_t)row_line[Y] * cw * 3], (size_t)cw * 3);
    }
    v.last_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

static bool save_frame(const Viewer &v, const std::string &path) {
    SDL_Surface *s = SDL_CreateSurfaceFrom(v.fw, v.fh, SDL_PIXELFORMAT_RGB24, (void *)v.frame.data(), v.fw * 3);
    if (!s) return false;
    bool ok = IMG_SavePNG(s, path.c_str());
    SDL_DestroySurface(s);
    if (ok) printf("saved %s (%dx%d, %s, %.1f ms)\n", path.c_str(), v.fw, v.fh, mode_name(v.mode), v.last_ms);
    else fprintf(stderr, "save %s failed: %s\n", path.c_str(), SDL_GetError());
    return ok;
}

static void usage() {
    fprintf(stderr,
            "usage: ntscview [--hgr|--dhgr] [--luma MHz] [--i MHz] [--q MHz]\n"
            "                [--trap] [--luma-taps N] [--mode 1|2|3]\n"
            "                [--size WxH --screenshot out.png] <file> [file...]\n");
}

int main(int argc, char **argv) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    int force_mode = 0;
    KernelParams kp;
    int luma_taps = 0;
    int shot_w = 0, shot_h = 0;
    std::string shot_path;
    Viewer v;
    std::vector<std::string> paths;
    for (int a = 1; a < argc; a++) {
        std::string arg = argv[a];
        if (arg == "--hgr") force_mode = 1;
        else if (arg == "--dhgr") force_mode = 2;
        else if (arg == "--luma" && a + 1 < argc) kp.luma_hz = (float)atof(argv[++a]) * 1e6f;
        else if (arg == "--i" && a + 1 < argc) kp.i_hz = (float)atof(argv[++a]) * 1e6f;
        else if (arg == "--q" && a + 1 < argc) kp.q_hz = (float)atof(argv[++a]) * 1e6f;
        else if (arg == "--trap") kp.trap = true;
        else if (arg == "--luma-taps" && a + 1 < argc) luma_taps = std::clamp(atoi(argv[++a]), 1, 64);
        else if (arg == "--mode" && a + 1 < argc) v.mode = std::clamp(atoi(argv[++a]), 1, 3);
        else if (arg == "--size" && a + 1 < argc) {
            if (sscanf(argv[++a], "%dx%d", &shot_w, &shot_h) != 2 || shot_w < 1 || shot_h < 1) { usage(); return 1; }
        } else if (arg == "--screenshot" && a + 1 < argc) shot_path = argv[++a];
        else if (arg.rfind("--", 0) == 0) { usage(); return 1; }
        else paths.push_back(arg);
    }
    if (paths.empty()) { usage(); return 1; }
    if (luma_taps) kp.luma_taps = luma_taps;
    if (!shot_path.empty() && shot_w == 0) { fprintf(stderr, "--screenshot requires --size\n"); return 1; }

    for (const auto &p : paths) {
        Source s;
        s.path = p;
        s.name = base_name(p);
        s.bs = new Bitstream();
        bool dhgr = false;
        size_t n = 0;
        if (!load_page(p, force_mode, *s.bs, dhgr, n)) {
            fprintf(stderr, "cannot open %s\n", p.c_str());
            delete s.bs;
            continue;
        }
        v.sources.push_back(s);
    }
    if (v.sources.empty()) return 1;

    setupConfig();
    generate_filters(NUM_TAPS);
    config.heldDots = false;
    init_hgr_LUT();
    memcpy(v.lut_legacy.data(), g_hgr_LUT, sizeof(RGBA_t) * v.lut_legacy.size());
    v.kernel.build(kp, config.subcarrier, config.phaseInfo[0]);
    {
        // at whole-dot positions the production settings must reproduce the production held weights
        KernelParams pp;
        if (config.lumaTrap) {
            pp.luma_hz = 3'500'000.0f;
            pp.i_hz = 2'000'000.0f;
            pp.q_hz = 1'300'000.0f;
            pp.trap = true;
        }
        ContinuousKernel prod;
        prod.build(pp, config.subcarrier, config.phaseInfo[0]);
        float maxd = 0;
        for (int k = -NUM_TAPS; k <= NUM_TAPS; k++)
            for (int p = 0; p < 4; p++)
                for (int c = 0; c < 3; c++)
                    maxd = std::max(maxd, std::fabs(prod.at(0, k, p)[c] - config.heldWeights[k + NUM_TAPS][p][c]));
        printf("kernel: luma %.2f MHz (+/-%d dots)%s, I %.2f MHz, Q %.2f MHz; "
               "production max |K(0) - heldWeights| = %.2e\n",
               kp.luma_hz / 1e6f, kp.luma_taps,
               kp.trap ? " gaussian + trap" : "",
               kp.i_hz / 1e6f, kp.q_hz / 1e6f, maxd);

        printf("luma response:");
        for (float mhz : {1.0f, 2.0f, 2.5f, 3.0f, 3.25f, kFsc / 1e6f, 4.0f, 4.5f}) {
            double h = 0;
            for (int k = v.kernel.k_min; k < v.kernel.k_min + v.kernel.k_count; k++)
                h += v.kernel.at(0, k, 0)[0] * cos(2 * M_PI * mhz * 1e6 / kFsc4 * k);
            printf("  %.2f MHz %.1f dB", mhz, 20 * log10(std::max(std::fabs(h), 1e-9)));
        }
        // luma step response sampled at dot centres
        double acc = 0, over = 0, under = 0;
        for (int k = v.kernel.k_min; k < v.kernel.k_min + v.kernel.k_count; k++) {
            acc += v.kernel.at(0, k, 0)[0];
            over = std::max(over, acc - 1.0);
            under = std::min(under, acc);
        }
        printf("\nluma step: overshoot %.1f%%, undershoot %.1f%%\n", over * 100, -under * 100);
    }

    if (!shot_path.empty()) {
        render_frame(v, shot_w, shot_h);
        return save_frame(v, shot_path) ? 0 : 1;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("ntscview", 1288, 928, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); return 1; }

    SDL_Texture *tex = nullptr;
    int tex_w = 0, tex_h = 0;
    bool dirty = true;
    bool running = true;

    auto present = [&]() {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        if (tex) SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    };

    auto rerender = [&]() {
        int w = 0, h = 0;
        SDL_GetCurrentRenderOutputSize(ren, &w, &h);
        if (w < 1 || h < 1) return;
        render_frame(v, w, h);
        if (!tex || tex_w != w || tex_h != h) {
            if (tex) SDL_DestroyTexture(tex);
            tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, w, h);
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            tex_w = w;
            tex_h = h;
        }
        SDL_UpdateTexture(tex, nullptr, v.frame.data(), w * 3);
        char title[256];
        snprintf(title, sizeof(title), "ntscview - %s - %s - %dx%d px (image %dx%d) - %.1f ms",
                 v.sources[v.cur].name.c_str(), mode_name(v.mode), w, h, v.cw, v.ch, v.last_ms);
        SDL_SetWindowTitle(win, title);
        present();
    };

    while (running) {
        if (dirty) {
            rerender();
            dirty = false;
        }
        SDL_Event e;
        // a timeout so signal-generated SDL_EVENT_QUIT is noticed while idle
        if (!SDL_WaitEventTimeout(&e, 100)) continue;
        do {
            switch (e.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    dirty = true;
                    break;
                case SDL_EVENT_WINDOW_EXPOSED:
                    present();
                    break;
                case SDL_EVENT_KEY_DOWN:
                    switch (e.key.key) {
                        case SDLK_ESCAPE: running = false; break;
                        case SDLK_1: v.mode = MODE_EXACT; dirty = true; break;
                        case SDLK_2: v.mode = MODE_LANCZOS; dirty = true; break;
                        case SDLK_3: v.mode = MODE_NEAREST; dirty = true; break;
                        case SDLK_RIGHT:
                            v.cur = (v.cur + 1) % (int)v.sources.size();
                            dirty = true;
                            break;
                        case SDLK_LEFT:
                            v.cur = (v.cur + (int)v.sources.size() - 1) % (int)v.sources.size();
                            dirty = true;
                            break;
                        case SDLK_S: {
                            char path[512];
                            snprintf(path, sizeof(path), "/tmp/ntscview_%s_%s_%dx%d.png",
                                     v.sources[v.cur].name.c_str(), mode_name(v.mode), v.fw, v.fh);
                            save_frame(v, path);
                            break;
                        }
                        default: break;
                    }
                    break;
                default: break;
            }
        } while (SDL_PollEvent(&e));
    }

    if (tex) SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
