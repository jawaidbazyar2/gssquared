/*
 * vsgtest: equivalence and timing for the composite video path.
 *
 * Feeds identical scan streams through the frozen byte-per-dot reference
 * (RefComposite.hpp) and the packed-bitstream VideoScanGenerator_Comp, and
 * requires bit-identical 910x263 output from both NTSC560 and Monochrome560.
 *
 *   vsgtest          run all checks (exit 1 on any failure)
 *   vsgtest --bench  also time generate+render and render alone, old vs new
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "devices/displaypp/CharRom.hpp"
#include "devices/displaypp/ScanBuffer.hpp"
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/VideoScannerIIe.hpp"
#include "devices/displaypp/VideoScannerIIgs.hpp"
#include "devices/displaypp/VideoScanGenerator_Comp.hpp"
#include "devices/displaypp/render/Monochrome560.hpp"
#include "devices/displaypp/render/NTSC560.hpp"
#include "mmus/mmu_iie.hpp"

#include "RefComposite.hpp"

namespace {

constexpr uint32_t W = 910;
constexpr uint32_t H = 263;
constexpr uint32_t CYCLES_PER_FRAME = 17030;
const RGBA_t SENTINEL = RGBA_t::make(0xDE, 0xAD, 0xBE, 0xEF);

enum ScannerKind { SC_II, SC_IIE, SC_IIGS };
const char *scanner_names[] = { "II", "IIe", "IIgs" };

struct Scenario {
    std::string name;
    ScannerKind scanner;
    std::function<void(VideoScannerII *)> setup;
    // Called before each video cycle with the cycle index within the frame.
    std::function<void(VideoScannerII *, uint32_t, std::mt19937 &)> per_cycle;
    bool dhgr_mono = false;
    int frames = 32;
};

int failures = 0;

bool compare(const std::vector<RGBA_t> &want, const std::vector<RGBA_t> &got, const std::string &name,
             int frame, const char *engine) {
    for (uint32_t i = 0; i < W * H; i++) {
        if (want[i] != got[i]) {
            failures++;
            printf("\n  FAIL %s frame %d %s: first diff at (%u, %u) expected %08X got %08X\n",
                   name.c_str(), frame, engine, i % W, i / W, want[i].rgba, got[i].rgba);
            return false;
        }
    }
    return true;
}

bool is_black(RGBA_t p) { return p == RGBA_t::make(0, 0, 0, 0xFF); }

uint32_t count_nonblack(const std::vector<RGBA_t> &buf, uint32_t x0, uint32_t x1) {
    uint32_t n = 0;
    for (uint32_t y = 35; y < 35 + 192; y++)
        for (uint32_t x = x0; x <= x1; x++)
            if (!is_black(buf[y * W + x])) n++;
    return n;
}

// Pulls everything the scanner produced into both pipeline buffers.
void split_stream(ScanBuffer *src, ScanBuffer *a, ScanBuffer *b) {
    uint32_t n = src->get_count();
    while (n--) {
        Scan_t s = src->pull();
        a->push(s);
        b->push(s);
    }
}

VideoScannerII *make_scanner(ScannerKind k, MMU_IIe *mmu) {
    VideoScannerII *s = nullptr;
    switch (k) {
        case SC_II: s = new VideoScannerII(mmu); break;
        case SC_IIE: s = new VideoScannerIIe(mmu); break;
        case SC_IIGS: s = new VideoScannerIIgs(mmu); break;
    }
    s->initialize();
    return s;
}

struct Context {
    MMU_IIe *mmu;
    CharRom *rom_ii;
    CharRom *rom_iie;
    NTSC560 *ntsc;
    Monochrome560 *mono;
};

void fill_ram(MMU_IIe *mmu, uint32_t seed) {
    std::mt19937 rng(seed);
    uint8_t *ram = mmu->get_memory_base();
    for (uint32_t i = 0; i < 0x20000; i++) ram[i] = (uint8_t)rng();
}

// Returns the per-frame outputs of the last frame through out params when requested.
void run_scenario(Context &ctx, const Scenario &sc, uint32_t seed,
                  std::vector<RGBA_t> *last_new_ntsc = nullptr) {
    printf("%-44s [%s] ", sc.name.c_str(), scanner_names[sc.scanner]);
    fflush(stdout);
    int before = failures;

    fill_ram(ctx.mmu, seed);
    std::mt19937 rng(seed ^ 0x9E3779B9u);

    bool shift = (sc.scanner == SC_IIE);
    CharRom *rom = (sc.scanner == SC_II) ? ctx.rom_ii : ctx.rom_iie;

    VideoScannerII *scanner = make_scanner(sc.scanner, ctx.mmu);
    if (sc.setup) sc.setup(scanner);

    auto bufRef = std::make_unique<ScanBuffer>();
    auto bufNew = std::make_unique<ScanBuffer>();

    ref::RefComp refc(rom);
    ref::RefNTSC560 rntsc;
    ref::RefMono560 rmono;
    rntsc.shift_enabled = shift;
    rmono.shift_enabled = shift;
    refc.set_render(&rntsc);
    refc.set_dhgr_mono_mode(sc.dhgr_mono);

    VideoScanGenerator_Comp vsgc(rom, false, nullptr);
    vsgc.set_display_shift(shift);
    vsgc.set_dhgr_mono_mode(sc.dhgr_mono);
    ctx.ntsc->set_shift_enabled(shift);
    ctx.mono->set_shift_enabled(shift);

    std::vector<RGBA_t> refN(W * H), refM(W * H), newN(W * H), newM(W * H);

    // generate_frame hands its bits to Render::render, which needs an open FrameVSG.
    // Capture the bits instead and decode them into plain buffers.
    struct Capture : Render {
        const FrameBits *bits = nullptr;
        void render_lines(const FrameBits &b, RGBA_t *, uint32_t) override { bits = &b; }
    } capture;
    vsgc.set_render(&capture);

    for (int f = 0; f < sc.frames; f++) {
        for (uint32_t c = 0; c < CYCLES_PER_FRAME; c++) {
            if (sc.per_cycle) sc.per_cycle(scanner, c, rng);
            scanner->video_cycle();
        }
        split_stream(scanner->get_frame_scan(), bufRef.get(), bufNew.get());

        std::fill(refN.begin(), refN.end(), SENTINEL);
        std::fill(refM.begin(), refM.end(), SENTINEL);
        std::fill(newN.begin(), newN.end(), SENTINEL);
        std::fill(newM.begin(), newM.end(), SENTINEL);

        refc.generate_frame(bufRef.get(), refN.data(), W);
        rmono.render(refc.get_frame_byte(), refM.data(), W);

        vsgc.generate_frame(bufNew.get());
        ctx.ntsc->render_lines(*capture.bits, newN.data(), W);
        ctx.mono->render_lines(*capture.bits, newM.data(), W);

        bool ok = compare(refN, newN, sc.name, f, "NTSC") && compare(refM, newM, sc.name, f, "mono");
        if (!ok) break;
    }

    if (last_new_ntsc) *last_new_ntsc = newN;
    delete scanner;
    printf("%s\n", failures == before ? "ok" : "");
}

// Synthetic stream: one VSYNC-terminated frame built by the caller.
struct StreamBuilder {
    std::vector<Scan_t> s;
    void hsync() { s.push_back({ (uint8_t)VM_HSYNC, 0, 0, 0, 0 }); }
    void vsync() { s.push_back({ (uint8_t)VM_VSYNC, 0, 0, 0, 0 }); }
    void data(uint8_t mode, uint8_t aux, uint8_t main, uint8_t flags = 0) { s.push_back({ mode, aux, main, flags, 0 }); }
    void load(ScanBuffer *b) const { for (const Scan_t &x : s) b->push(x); }
};

void check_bounds(Context &ctx) {
    printf("%-44s ", "bounds: >40 scans per line, >192 lines");
    int before = failures;

    VideoScanGenerator_Comp vsgc(ctx.rom_iie, false, nullptr);
    vsgc.set_display_shift(true);
    struct Capture : Render {
        void render_lines(const FrameBits &, RGBA_t *, uint32_t) override {}
    } capture;
    vsgc.set_render(&capture);

    StreamBuilder sb;
    sb.hsync();
    for (int i = 0; i < 60; i++) sb.data(VM_HIRES, 0, 0x7F);  // line 0: 60 scans of all-ones
    sb.hsync();
    for (int i = 0; i < 40; i++) sb.data(VM_HIRES, 0, 0x00);  // line 1: all zeros
    for (int y = 2; y < 230; y++) {                            // run past the visible lines
        sb.hsync();
        for (int i = 0; i < 45; i++) sb.data(VM_DHIRES, 0x7F, 0x7F);
    }
    sb.vsync();

    auto buf = std::make_unique<ScanBuffer>();
    sb.load(buf.get());
    vsgc.generate_frame(buf.get());
    const FrameBits *bits = vsgc.get_frame_bits();

    // Line 0, phase 0 with shift: data starts at dot 7 and every dot through 575 is set.
    const uint32_t *l0 = bits->line(0);
    if (l0[0] != 0xFFFFFF80u || l0[17] != 0xFFFFFFFFu) {
        failures++;
        printf("\n  FAIL line 0 words: [0]=%08X [17]=%08X\n", l0[0], l0[17]);
    }
    const uint32_t *l1 = bits->line(1);
    for (uint32_t w = 0; w < FrameBits::DOT_WORDS; w++) {
        if (l1[w] != 0) {
            failures++;
            printf("\n  FAIL line 1 word %u = %08X (overrun from line 0)\n", w, l1[w]);
            break;
        }
    }
    if (bits->line_start(1) != 7 || bits->colorburst(1) != 1 || bits->phase_offset(1) != 0) {
        failures++;
        printf("\n  FAIL line 1 mode word %08X\n", l1[FrameBits::DOT_WORDS]);
    }
    const uint32_t *l191 = bits->line(191);
    if (l191[17] != 0xFFFFFFFFu || bits->line_start(191) != 0 || bits->phase_offset(191) != 1) {
        failures++;
        printf("\n  FAIL line 191 [17]=%08X mode %08X\n", l191[17], l191[FrameBits::DOT_WORDS]);
    }
    printf("%s\n", failures == before ? "ok" : "");
}

void check_geometry(Context &ctx) {
    // IIe: 80-column output reaches columns 161..167; 40-column output reaches 721..727.
    std::vector<RGBA_t> out;
    int before = failures;

    Scenario t80{ "geometry: IIe TEXT80 uses 161..167", SC_IIE, [](VideoScannerII *s) { s->set_text(); s->set_80col(); }, nullptr, false, 2 };
    run_scenario(ctx, t80, 101, &out);
    if (count_nonblack(out, 161, 167) == 0) { failures++; printf("  FAIL no 80-column output in 161..167\n"); }
    if (count_nonblack(out, 721, 727) != 0) { failures++; printf("  FAIL 80-column output in 721..727\n"); }

    Scenario t40{ "geometry: IIe TEXT40 uses 721..727", SC_IIE, [](VideoScannerII *s) { s->set_text(); }, nullptr, false, 2 };
    run_scenario(ctx, t40, 102, &out);
    if (count_nonblack(out, 721, 727) == 0) { failures++; printf("  FAIL no 40-column output in 721..727\n"); }
    if (count_nonblack(out, 161, 167) != 0) { failures++; printf("  FAIL 40-column output in 161..167\n"); }

    Scenario gs{ "geometry: IIgs TEXT80 stays in 168..727", SC_IIGS, [](VideoScannerII *s) { s->set_text(); s->set_80col(); }, nullptr, false, 2 };
    run_scenario(ctx, gs, 103, &out);
    if (out[35 * W + 167] != SENTINEL || out[35 * W + 728] != SENTINEL) { failures++; printf("  FAIL IIgs wrote outside 168..727\n"); }
    if (count_nonblack(out, 168, 174) == 0) { failures++; printf("  FAIL IIgs no output at 168..174\n"); }

    Scenario ii{ "geometry: II HIRES stays in 168..727", SC_II, [](VideoScannerII *s) { s->set_graf(); s->set_hires(); }, nullptr, false, 2 };
    run_scenario(ctx, ii, 104, &out);
    if (out[100 * W + 167] != SENTINEL || out[100 * W + 728] != SENTINEL) { failures++; printf("  FAIL II wrote outside 168..727\n"); }
    if (failures == before) printf("%-44s ok\n", "geometry checks");
}

std::vector<Scenario> scenarios() {
    using S = VideoScannerII;
    std::vector<Scenario> v;
    auto add = [&](const char *name, ScannerKind k, std::function<void(S *)> setup, bool dmono = false) {
        v.push_back({ name, k, setup, nullptr, dmono, 32 });
    };

    for (ScannerKind k : { SC_II, SC_IIE, SC_IIGS }) {
        add("TEXT40", k, [](S *s) { s->set_text(); });
        add("LORES", k, [](S *s) { s->set_graf(); s->set_lores(); });
        add("LORES mixed", k, [](S *s) { s->set_graf(); s->set_lores(); s->set_mixed(); });
        add("HIRES", k, [](S *s) { s->set_graf(); s->set_hires(); });
        add("HIRES mixed page 2", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_mixed(); s->set_page_2(); });
    }
    for (ScannerKind k : { SC_IIE, SC_IIGS }) {
        add("TEXT40 altcharset", k, [](S *s) { s->set_text(); s->set_altchrset(); });
        add("TEXT80", k, [](S *s) { s->set_text(); s->set_80col(); });
        add("TEXT80 altcharset", k, [](S *s) { s->set_text(); s->set_80col(); s->set_altchrset(); });
        add("LORES mixed 80", k, [](S *s) { s->set_graf(); s->set_lores(); s->set_mixed(); s->set_80col(); });
        add("HIRES mixed 80", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_mixed(); s->set_80col(); });
        add("DLORES", k, [](S *s) { s->set_graf(); s->set_lores(); s->set_80col(); s->set_dblres(); });
        add("DLORES mixed", k, [](S *s) { s->set_graf(); s->set_lores(); s->set_80col(); s->set_dblres(); s->set_mixed(); });
        add("DHIRES", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_80col(); s->set_dblres(); });
        add("DHIRES mixed", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_80col(); s->set_dblres(); s->set_mixed(); });
        add("DHIRES dhgr_mono_mode", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_80col(); s->set_dblres(); }, true);
        add("LORES_7M (dblres, 40 col)", k, [](S *s) { s->set_graf(); s->set_lores(); s->set_dblres(); });
        add("HIRES_NOSHIFT (dblres, 40 col)", k, [](S *s) { s->set_graf(); s->set_hires(); s->set_dblres(); });
    }

    // IIe screens mixing 40- and 80-column lines: flip 80COL every 8 lines.
    for (ScannerKind k : { SC_IIE, SC_IIGS }) {
        v.push_back({ "TEXT 40/80 alternating every 8 lines", k,
            [](S *s) { s->set_text(); },
            [](S *s, uint32_t c, std::mt19937 &) {
                if (c % 65 == 0) {
                    uint32_t line = c / 65;
                    if (line % 16 == 0) s->set_80col();
                    else if (line % 16 == 8) s->reset_80col();
                }
            }, false, 32 });
        v.push_back({ "HIRES/TEXT80 mixed, 80COL flips per line", k,
            [](S *s) { s->set_graf(); s->set_hires(); s->set_mixed(); },
            [](S *s, uint32_t c, std::mt19937 &) {
                if (c % 65 == 3) {
                    if ((c / 65) & 1) s->set_80col(); else s->reset_80col();
                }
            }, false, 32 });
    }

    // Random mid-line switch churn, including mid-line mode and column changes.
    for (ScannerKind k : { SC_II, SC_IIE, SC_IIGS }) {
        v.push_back({ "random switch churn", k, nullptr,
            [k](S *s, uint32_t, std::mt19937 &rng) {
                if (rng() % 97 != 0) return;
                bool on = rng() & 1;
                uint32_t which = rng() % ((k == SC_II) ? 4 : 7);
                switch (which) {
                    case 0: on ? s->set_graf() : s->set_text(); break;
                    case 1: on ? s->set_hires() : s->set_lores(); break;
                    case 2: on ? s->set_mixed() : s->set_full(); break;
                    case 3: on ? s->set_page_2() : s->set_page_1(); break;
                    case 4: s->set_80col_f(on); break;
                    case 5: s->set_dblres_f(on); break;
                    case 6: s->set_altchrset_f(on); break;
                }
            }, false, 64 });
    }
    return v;
}

// Captures exactly one generate_frame worth of scans: everything after a VSYNC up to and including the next.
std::vector<Scan_t> capture_frame(Context &ctx, ScannerKind k, const std::function<void(VideoScannerII *)> &setup) {
    fill_ram(ctx.mmu, 7);
    VideoScannerII *scanner = make_scanner(k, ctx.mmu);
    setup(scanner);
    std::vector<Scan_t> all;
    for (uint32_t c = 0; c < CYCLES_PER_FRAME * 3; c++) {
        scanner->video_cycle();
        ScanBuffer *sb = scanner->get_frame_scan();
        while (sb->get_count()) all.push_back(sb->pull());
    }
    delete scanner;
    std::vector<size_t> vs;
    for (size_t i = 0; i < all.size(); i++) if (all[i].mode == VM_VSYNC) vs.push_back(i);
    return std::vector<Scan_t>(all.begin() + vs[0] + 1, all.begin() + vs[1] + 1);
}

double ns_now() {
    return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

void bench(Context &ctx) {
    using S = VideoScannerII;
    struct B { const char *name; std::function<void(S *)> setup; };
    std::vector<B> modes = {
        { "TEXT40", [](S *s) { s->set_text(); } },
        { "TEXT80", [](S *s) { s->set_text(); s->set_80col(); } },
        { "LORES", [](S *s) { s->set_graf(); s->set_lores(); } },
        { "HIRES", [](S *s) { s->set_graf(); s->set_hires(); } },
        { "DHIRES", [](S *s) { s->set_graf(); s->set_hires(); s->set_80col(); s->set_dblres(); } },
    };
    constexpr int ITER = 2000;

    printf("\nbench (IIe, microseconds per frame, median of %d)\n", ITER);
    printf("%-8s %12s %12s %12s %12s %12s %12s\n", "mode", "old gen+ntsc", "new gen+ntsc",
           "old ntsc", "new ntsc", "old mono", "new mono");

    std::vector<RGBA_t> out(W * H);
    auto buf = std::make_unique<ScanBuffer>();
    auto median = [](std::vector<double> &t) { std::sort(t.begin(), t.end()); return t[t.size() / 2] / 1000.0; };

    for (auto &m : modes) {
        std::vector<Scan_t> frame = capture_frame(ctx, SC_IIE, m.setup);

        ref::RefComp refc(ctx.rom_iie);
        ref::RefNTSC560 rntsc; ref::RefMono560 rmono;
        refc.set_render(&rntsc);
        VideoScanGenerator_Comp vsgc(ctx.rom_iie, false, nullptr);
        vsgc.set_display_shift(true);
        ctx.ntsc->set_shift_enabled(true);
        ctx.mono->set_shift_enabled(true);
        struct Fwd : Render {
            Render *to; RGBA_t *out;
            void render_lines(const FrameBits &b, RGBA_t *, uint32_t p) override { to->render_lines(b, out, 910); (void)p; }
        } fwd;
        fwd.to = ctx.ntsc; fwd.out = out.data();
        vsgc.set_render(&fwd);

        std::vector<double> t_old(ITER), t_new(ITER), r_old(ITER), r_new(ITER), m_old(ITER), m_new(ITER);
        for (int i = 0; i < ITER; i++) {
            buf->clear(); for (auto &s : frame) buf->push(s);
            double t0 = ns_now(); refc.generate_frame(buf.get(), out.data(), W); t_old[i] = ns_now() - t0;
            buf->clear(); for (auto &s : frame) buf->push(s);
            t0 = ns_now(); vsgc.generate_frame(buf.get()); t_new[i] = ns_now() - t0;
        }
        for (int i = 0; i < ITER; i++) {
            double t0 = ns_now(); rntsc.render(refc.get_frame_byte(), out.data(), W); r_old[i] = ns_now() - t0;
            t0 = ns_now(); ctx.ntsc->render_lines(*vsgc.get_frame_bits(), out.data(), W); r_new[i] = ns_now() - t0;
            t0 = ns_now(); rmono.render(refc.get_frame_byte(), out.data(), W); m_old[i] = ns_now() - t0;
            t0 = ns_now(); ctx.mono->render_lines(*vsgc.get_frame_bits(), out.data(), W); m_new[i] = ns_now() - t0;
        }
        printf("%-8s %12.1f %12.1f %12.1f %12.1f %12.1f %12.1f\n", m.name,
               median(t_old), median(t_new), median(r_old), median(r_new), median(m_old), median(m_new));
    }
}

} // namespace

int main(int argc, char **argv) {
    bool do_bench = argc > 1 && strcmp(argv[1], "--bench") == 0;

    CharRom rom_ii("resources/roms/apple2_plus/char.rom");
    CharRom rom_iie("resources/roms/apple2e_enh/char.rom");
    if (!rom_ii.is_valid() || !rom_iie.is_valid()) {
        printf("Failed to load char roms (run from the build directory layout)\n");
        return 2;
    }

    uint8_t *rom = new uint8_t[12 * 1024]();
    MMU_IIe *mmu = new MMU_IIe(128, 128 * 1024, rom);

    NTSC560 ntsc;        // also builds g_hgr_LUT, shared with the reference decoder
    Monochrome560 mono;
    Context ctx{ mmu, &rom_ii, &rom_iie, &ntsc, &mono };

    uint32_t seed = 1;
    for (const Scenario &sc : scenarios()) run_scenario(ctx, sc, seed++);
    check_geometry(ctx);
    check_bounds(ctx);

    if (failures) {
        printf("\n%d FAILURE(S)\n", failures);
        return 1;
    }
    printf("\nall checks passed\n");

    if (do_bench) bench(ctx);
    return 0;
}
