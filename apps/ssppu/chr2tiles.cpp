/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Convert NES .chr (2bpp planar, 16 bytes/tile) to 8bpp tile format
 *   (64 bytes/tile, palette indices 0-3). Index 0 is transparent in PPU mode.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static constexpr int CHR_BYTES_PER_TILE = 16;
static constexpr int OUT_BYTES_PER_TILE = 64;
static constexpr int TILE_W = 8;
static constexpr int TILE_H = 8;

static bool read_file(const char *path, std::vector<uint8_t> &out) {
    FILE *f = std::fopen(path, "rb");
    if (f == nullptr) {
        std::fprintf(stderr, "chr2tiles: cannot open input '%s'\n", path);
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    if (size < 0) {
        std::fclose(f);
        std::fprintf(stderr, "chr2tiles: cannot stat input '%s'\n", path);
        return false;
    }
    std::fseek(f, 0, SEEK_SET);
    out.resize((size_t)size);
    if (size > 0) {
        const size_t got = std::fread(out.data(), 1, (size_t)size, f);
        if (got != (size_t)size) {
            std::fclose(f);
            std::fprintf(stderr, "chr2tiles: short read on input '%s'\n", path);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

static bool write_file(const char *path, const uint8_t *data, size_t size) {
    FILE *f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "chr2tiles: cannot open output '%s'\n", path);
        return false;
    }
    if (size > 0) {
        const size_t wrote = std::fwrite(data, 1, size, f);
        if (wrote != size) {
            std::fclose(f);
            std::fprintf(stderr, "chr2tiles: short write on output '%s'\n", path);
            return false;
        }
    }
    std::fclose(f);
    return true;
}

static void decode_chr_tile(const uint8_t *chr_tile, uint8_t *out_tile) {
    const uint8_t *plane0 = chr_tile;
    const uint8_t *plane1 = chr_tile + 8;

    for (int y = 0; y < TILE_H; y++) {
        const uint8_t row0 = plane0[y];
        const uint8_t row1 = plane1[y];
        for (int x = 0; x < TILE_W; x++) {
            const int bit = 7 - x;
            const uint8_t lo = (uint8_t)((row0 >> bit) & 1);
            const uint8_t hi = (uint8_t)((row1 >> bit) & 1);
            out_tile[(size_t)y * TILE_W + (size_t)x] = (uint8_t)(lo | (hi << 1));
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s input.chr output.tiles\n", argv[0]);
        return 1;
    }

    std::vector<uint8_t> chr_data;
    if (!read_file(argv[1], chr_data)) {
        return 1;
    }

    if (chr_data.empty()) {
        std::fprintf(stderr, "chr2tiles: input file is empty\n");
        return 1;
    }

    const size_t partial = chr_data.size() % CHR_BYTES_PER_TILE;
    if (partial != 0) {
        std::fprintf(stderr, "chr2tiles: warning: ignoring %zu trailing byte(s)\n", partial);
        chr_data.resize(chr_data.size() - partial);
    }

    const size_t tile_count = chr_data.size() / CHR_BYTES_PER_TILE;
    std::vector<uint8_t> out_data(tile_count * OUT_BYTES_PER_TILE);

    for (size_t t = 0; t < tile_count; t++) {
        decode_chr_tile(chr_data.data() + t * CHR_BYTES_PER_TILE,
            out_data.data() + t * OUT_BYTES_PER_TILE);
    }

    if (!write_file(argv[2], out_data.data(), out_data.size())) {
        return 1;
    }

    std::printf("chr2tiles: converted %zu tiles (%zu bytes -> %zu bytes)\n",
        tile_count, chr_data.size(), out_data.size());
    return 0;
}
