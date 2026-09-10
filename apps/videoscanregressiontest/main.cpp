#include <cstdio>
#include <cstdlib>
#include <memory>

#include <SDL3/SDL.h>
#include "devices/displaypp/VideoScannerII.hpp"
#include "devices/displaypp/VideoScanGenerator_Comp.hpp"
#include "devices/displaypp/VideoScanGenerator_RGB.hpp"
#include "devices/displaypp/render/Monochrome560.hpp"

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
}

void samples(ScanBuffer& scans, int count, uint8_t value) {
    for (int i = 0; i < count; ++i)
        scans.push({VM_LORES, 0, value, 0, 0});
}

void partial_scanline(VideoScanGenerator_Comp& decoder, ScanBuffer& scans,
                      FrameVSG& output) {
    samples(scans, 20, 0xFF);
    decoder.generate_frame(&scans);
    samples(scans, 20, 0x00);
    decoder.generate_frame(&scans);
    const RGBA_t* row = output.data() + 35 * FrameVSG::max_width() + 168;
    for (int x = 0; x < 560; ++x)
        expect(row[x].r == (x < 280 ? 255 : 0),
               "rendering a paused partial scanline preserves the write cursor");
}

void marker(ScanBuffer& scans, uint8_t mode) {
    scans.push({mode, 0, 0, 0, 0});
}

bool row_is(FrameVSG& output, size_t line, size_t start, uint8_t red) {
    const RGBA_t* row = output.data() + line * FrameVSG::max_width() + start;
    for (size_t x = 0; x < 560; ++x) if (row[x].r != red) return false;
    return true;
}

void discarded_samples(VideoScanGeneratorIntf& decoder, ScanBuffer& scans,
                        FrameVSG& output, size_t line, size_t start) {
    marker(scans, VM_VSYNC);
    decoder.generate_frame(&scans);
    samples(scans, 40, 0xFF);
    decoder.generate_frame(&scans);
    expect(row_is(output, line, start, 255), "initial visible scanline is white");

    // A discarded frame may contain VSync. Resuming the decoder must retain
    // its image until a new sync is observed.
    scans.clear();
    samples(scans, 40, 0x00);
    decoder.generate_frame(&scans);
    expect(row_is(output, line, start, 255),
           "discarded samples do not append to stale decoder state");
    marker(scans, VM_VSYNC);
    samples(scans, 40, 0x00);
    decoder.generate_frame(&scans);
    decoder.generate_frame(&scans);
    expect(row_is(output, line, start, 0), "decoder resumes at the next VSync");

    // Switching RGB/composite also transfers ownership of a shared ScanBuffer.
    // Its ring position can wrap, so use a monotonic read sequence for detection.
    samples(scans, 7, 0xFF);
    for (int i = 0; i < 7; ++i) scans.pull();
    samples(scans, 40, 0xFF);
    decoder.generate_frame(&scans);
    expect(row_is(output, line, start, 0),
           "another decoder consuming samples invalidates stale beam state");
    marker(scans, VM_VSYNC);
    samples(scans, 40, 0xFF);
    decoder.generate_frame(&scans);
    decoder.generate_frame(&scans);
    expect(row_is(output, line, start, 255), "engine handoff resumes after VSync");
}

void composite_boundaries(VideoScanGenerator_Comp& decoder, ScanBuffer& scans,
                          FrameVSG& output) {
    marker(scans, VM_VSYNC);
    decoder.generate_frame(&scans);
    for (int y = 0; y < 312; ++y) {
        samples(scans, 40, 0xFF);
        samples(scans, 20, 0x00); // Excess visible samples cannot spill to a row.
        marker(scans, VM_HSYNC);
    }
    decoder.generate_frame(&scans);
    expect(row_is(output, 35, 168, 255) && row_is(output, 226, 168, 255),
           "legacy decoder clips visible samples to 560 by 192");
    marker(scans, VM_VSYNC);
    samples(scans, 40, 0x00);
    decoder.generate_frame(&scans);
    decoder.generate_frame(&scans);
    expect(row_is(output, 35, 168, 0), "legacy boundary clipping preserves sync");
}

void rgb_boundaries(VideoScanGenerator_RGB& decoder, ScanBuffer& scans) {
    marker(scans, VM_VSYNC);
    decoder.generate_frame(&scans);
    for (int y = 0; y < 312; ++y) {
        samples(scans, 59, 0xFF);
        // C029 can change sample width from 14 to 16 pixels in a partial line.
        for (int x = 0; x < 10; ++x) marker(scans, VM_SHR);
        marker(scans, VM_HSYNC);
    }
    decoder.generate_frame(&scans);
    marker(scans, VM_VSYNC);
    decoder.generate_frame(&scans);
    expect(decoder.get_v() == 0, "RGB boundary clipping preserves VSync");
}
}

int main() {
    SDL_Surface* surface = SDL_CreateSurface(910, 263, SDL_PIXELFORMAT_RGBA8888);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) {
        std::fprintf(stderr, "SDL test renderer: %s\n", SDL_GetError());
        return 1;
    }
    {
        FrameVSG output(910, 263, renderer, SDL_PIXELFORMAT_RGBA8888);
        output.open();
        VideoScanGenerator_Comp decoder(nullptr, false, &output);
        Monochrome560 monitor;
        decoder.set_render(&monitor);
        auto scans = std::make_unique<ScanBuffer>();
        partial_scanline(decoder, *scans, output);
        discarded_samples(decoder, *scans, output, 35, 168);
        composite_boundaries(decoder, *scans, output);
        scans->clear();
        VideoScanGenerator_RGB rgb(nullptr, false, &output);
        discarded_samples(rgb, *scans, output, 0, 0);
        rgb_boundaries(rgb, *scans);
        output.close();
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    if (!failures) std::puts("Video scanner regression checks passed");
    return failures ? 1 : 0;
}
