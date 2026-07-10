/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "util/SystemSettings.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "paths.hpp"
#include "util/toml.hpp"

namespace {

int64_t now_unix_seconds() {
    using clock = std::chrono::system_clock;
    return std::chrono::duration_cast<std::chrono::seconds>(clock::now().time_since_epoch()).count();
}

bool file_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}

}  // namespace

SystemSettings& SystemSettings::instance() {
    static SystemSettings settings;
    return settings;
}

std::string SystemSettings::settings_path() {
    std::string path;
    Paths::calc_pref(path, "system_settings.toml");
    return path;
}

std::string SystemSettings::normalize_path(const std::string& path) {
    if (path.empty()) {
        return path;
    }
    std::error_code ec;
    std::filesystem::path p = std::filesystem::absolute(path, ec);
    if (ec) {
        return path;
    }
    p = std::filesystem::weakly_canonical(p, ec);
    if (ec) {
        return std::filesystem::absolute(path).lexically_normal().string();
    }
    return p.string();
}

void SystemSettings::trim_to_max() {
    if (static_cast<int>(recent_.size()) <= kMaxStored) {
        return;
    }
    std::sort(recent_.begin(), recent_.end(),
              [](const RecentConfigEntry& a, const RecentConfigEntry& b) {
                  return a.score > b.score;
              });
    recent_.resize(static_cast<size_t>(kMaxStored));
}

void SystemSettings::prune_missing() {
    recent_.erase(std::remove_if(recent_.begin(), recent_.end(),
                                 [](const RecentConfigEntry& e) {
                                     return e.path.empty() || !file_exists(e.path);
                                 }),
                  recent_.end());
}

bool SystemSettings::load() {
    recent_.clear();
    const std::string path = settings_path();
    if (!file_exists(path)) {
        return true;
    }

    try {
        const toml::table table = toml::parse_file(path);
        const auto version = table["gs2_settings_version"].value_or(0);
        if (version != 1) {
            std::cerr << "system_settings.toml: unsupported gs2_settings_version "
                      << version << std::endl;
            return false;
        }

        if (const auto* arr = table["recent_configs"].as_array()) {
            for (const auto& node : *arr) {
                const auto* entry = node.as_table();
                if (entry == nullptr) {
                    continue;
                }
                RecentConfigEntry e;
                if (const auto p = (*entry)["path"].value<std::string>()) {
                    e.path = *p;
                }
                e.score = (*entry)["score"].value_or(0.0);
                e.last_used = (*entry)["last_used"].value_or(static_cast<int64_t>(0));
                if (!e.path.empty()) {
                    recent_.push_back(std::move(e));
                }
            }
        }
    } catch (const toml::parse_error& err) {
        std::cerr << "Failed to parse system_settings.toml: " << err.what() << std::endl;
        recent_.clear();
        return false;
    }

    prune_missing();
    return true;
}

bool SystemSettings::save() const {
    toml::table table;
    table.insert("gs2_settings_version", 1);

    toml::array arr;
    for (const auto& e : recent_) {
        toml::table entry;
        entry.insert("path", e.path);
        entry.insert("score", e.score);
        entry.insert("last_used", e.last_used);
        arr.push_back(std::move(entry));
    }
    table.insert("recent_configs", std::move(arr));

    const std::string path = settings_path();
    try {
        std::ofstream out(path);
        if (!out) {
            std::cerr << "Failed to open system_settings.toml for writing: " << path << std::endl;
            return false;
        }
        out << table;
        return static_cast<bool>(out);
    } catch (const std::exception& ex) {
        std::cerr << "Failed to write system_settings.toml: " << ex.what() << std::endl;
        return false;
    }
}

void SystemSettings::record_use(const std::string& path) {
    const std::string normalized = normalize_path(path);
    if (normalized.empty()) {
        return;
    }

    for (auto& e : recent_) {
        e.score *= kDecay;
    }

    const int64_t now = now_unix_seconds();
    auto it = std::find_if(recent_.begin(), recent_.end(),
                           [&](const RecentConfigEntry& e) { return e.path == normalized; });
    if (it != recent_.end()) {
        it->score += 1.0;
        it->last_used = now;
    } else {
        RecentConfigEntry e;
        e.path = normalized;
        e.score = 1.0;
        e.last_used = now;
        recent_.push_back(std::move(e));
    }

    trim_to_max();
    save();
}

std::vector<RecentConfigEntry> SystemSettings::display_entries() const {
    std::vector<RecentConfigEntry> candidates;
    candidates.reserve(recent_.size());
    for (const auto& e : recent_) {
        if (!e.path.empty() && file_exists(e.path)) {
            candidates.push_back(e);
        }
    }
    if (candidates.empty()) {
        return {};
    }

    auto mru_it = std::max_element(
        candidates.begin(), candidates.end(),
        [](const RecentConfigEntry& a, const RecentConfigEntry& b) {
            return a.last_used < b.last_used;
        });

    std::vector<RecentConfigEntry> out;
    out.reserve(static_cast<size_t>(kMaxDisplay));
    out.push_back(*mru_it);

    std::vector<RecentConfigEntry> others;
    others.reserve(candidates.size());
    for (const auto& e : candidates) {
        if (e.path != mru_it->path) {
            others.push_back(e);
        }
    }
    std::sort(others.begin(), others.end(),
              [](const RecentConfigEntry& a, const RecentConfigEntry& b) {
                  if (a.score != b.score) {
                      return a.score > b.score;
                  }
                  return a.last_used > b.last_used;
              });

    for (const auto& e : others) {
        if (static_cast<int>(out.size()) >= kMaxDisplay) {
            break;
        }
        out.push_back(e);
    }
    return out;
}
