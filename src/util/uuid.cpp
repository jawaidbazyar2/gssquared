/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "util/uuid.hpp"

#include <chrono>
#include <cstdio>
#include <random>

std::string generate_uuid_v4() {
    uint8_t bytes[16];
    try {
        std::random_device rd;
        for (int i = 0; i < 16; ++i) {
            bytes[i] = static_cast<uint8_t>(rd());
        }
    } catch (...) {
        const auto seed = static_cast<unsigned>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::mt19937 gen(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        for (int i = 0; i < 16; ++i) {
            bytes[i] = static_cast<uint8_t>(dist(gen));
        }
    }

    // Version 4
    bytes[6] = static_cast<uint8_t>((bytes[6] & 0x0F) | 0x40);
    // Variant 10xx
    bytes[8] = static_cast<uint8_t>((bytes[8] & 0x3F) | 0x80);

    char out[37];
    std::snprintf(out, sizeof(out),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3],
                  bytes[4], bytes[5],
                  bytes[6], bytes[7],
                  bytes[8], bytes[9],
                  bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(out);
}
