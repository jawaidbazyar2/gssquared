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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct RecentConfigEntry {
    std::string path;
    double score = 0.0;
    int64_t last_used = 0;  // unix seconds
};

/** Kind of file dialog whose last path is tracked in SystemSettings. */
enum class FileDialogKind { Config, Disk };

/**
 * App-level settings (PrefPath/system_settings.toml). Distinct from SystemConfig
 * machine profiles. Tracks recently/frequently opened config files and HUD /
 * game-controller preferences.
 */
class SystemSettings {
    std::vector<RecentConfigEntry> recent_;

    bool hud_stats_ = false;
    bool hud_drives_ = true;
    bool disconnected_when_no_gamepad_ = false;

    /** Last .gs2 open/save selection (full file path when known). */
    std::string last_config_path_;
    /** Last disk-image open selection (full file path when known). */
    std::string last_disk_path_;

    /** Host FST shared folder (empty = Documents). */
    std::string host_fst_dir_;

    SystemSettings() = default;

    static std::string settings_path();
    static std::string normalize_path(const std::string& path);
    void trim_to_max();
    void prune_missing();

public:
    static constexpr int kMaxStored = 20;
    static constexpr int kMaxDisplay = 5;
    static constexpr double kDecay = 0.9;

    static SystemSettings& instance();

    /** Load from PrefPath/system_settings.toml (missing file is OK). */
    bool load();

    /** Write PrefPath/system_settings.toml. */
    bool save() const;

    /**
     * Decay all scores, bump this path, update last_used, trim to kMaxStored, save.
     */
    void record_use(const std::string& path);

    /**
     * Up to kMaxDisplay entries: [0] = MRU by last_used; [1..] = highest score
     * excluding the MRU. Skips missing files.
     */
    std::vector<RecentConfigEntry> display_entries() const;

    const std::vector<RecentConfigEntry>& recent() const { return recent_; }

    bool hud_stats() const { return hud_stats_; }
    bool hud_drives() const { return hud_drives_; }
    bool disconnected_when_no_gamepad() const { return disconnected_when_no_gamepad_; }

    void set_hud_stats(bool enabled);
    void set_hud_drives(bool enabled);
    void set_disconnected_when_no_gamepad(bool enabled);

    void toggle_hud_stats();
    void toggle_hud_drives();
    void toggle_disconnected_when_no_gamepad();

    const std::string& last_config_path() const { return last_config_path_; }
    const std::string& last_disk_path() const { return last_disk_path_; }
    const std::string& host_fst_dir() const { return host_fst_dir_; }

    void set_last_config_path(const std::string& path);
    void set_last_disk_path(const std::string& path);
    void set_host_fst_dir(const std::string& path);

    /**
     * Platform-adjusted default_location for SDL open dialogs.
     * Falls back to Documents on Linux for Disk when unset.
     */
    std::string get_file_dialog_default_location(FileDialogKind kind) const;

    /** Save-dialog default: last config directory + suggested_filename. */
    std::string get_file_dialog_save_default_location(const std::string& suggested_filename) const;

    /** Persist full selected/saved path for the given dialog kind. */
    void remember_file_dialog_selection(FileDialogKind kind, const std::string& selected_path);

    /** Seed a dialog kind with a directory only if that kind has no path yet. */
    void set_file_dialog_dir_if_unset(FileDialogKind kind, const std::string& dir);
};
