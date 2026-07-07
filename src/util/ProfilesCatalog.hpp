/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   Profiles.txt catalog loader — Phase 2.
 *   See Profiles Specification v0.81 (Michael Neil).
 */

#pragma once

#include <string>

// Phase 2: parse Profiles.txt entries (Settings.txt / nested Profiles.txt refs).
class ProfilesCatalog {
public:
    bool load(const std::string& path, std::string& error_out);
};
