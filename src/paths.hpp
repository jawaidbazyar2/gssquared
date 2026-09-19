/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <string>
#include <string_view>

const std::string& get_base_path(bool console_mode);
const std::string& get_pref_path(void);


class Paths {
    static std::string base_path;
    static std::string pref_path;
    static std::string docs_folder;
    static std::string home_folder;
    static std::string desktop_folder;
    static std::string user_systems_path;

    public:
        static void initialize(bool console_mode);
        static void calc_base(std::string& return_path, std::string file) ;
        static void calc_pref(std::string& return_path, std::string file) ;
        static void calc_docs(std::string& return_path, std::string file) ;
        static void calc_home(std::string& return_path, std::string file) ;
        static void calc_desktop(std::string& return_path, std::string file);
        static void calc_user_systems(std::string& return_path, std::string file);
        /** Full Desktop path for a new screenshot, e.g. ".../GS2 Screenshot 2026-07-17 14.30.05.png". */
        static std::string make_screenshot_path();

        /**
         * Platform-adjusted default_location for SDL open dialogs given a stored
         * last path (file or directory). Empty input → empty output.
         * Windows: full file path when known; directory with trailing sep otherwise.
         * macOS/Linux: directory only.
         */
        static std::string adapt_open_dialog_location(const std::string& stored_path);

        /**
         * Build a save-dialog default path from a last config path (file or dir)
         * and a suggested filename (e.g. "My_System.gs2").
         */
        static std::string make_save_dialog_location(const std::string& last_config_path,
                                                    const std::string& suggested_filename);

        /** Documents folder (set by initialize). */
        static const std::string& documents_folder() { return docs_folder; }

        /**
         * User-visible machine-profile folder: Documents/GSSquared/
         * (MEMFS root on Emscripten). Set by initialize().
         */
        static const std::string& user_systems_dir() { return user_systems_path; }

        /** Sidecar BRAM path for a .gs2: same directory, stem + ".bram". */
        static std::string bram_sidecar_path(const std::string& gs2_path);

        /** Legacy PrefPath/bram/<id>.bin (or bram/default.bin if id is empty). */
        static std::string legacy_pref_bram_path(const std::string& machine_id);

        /**
         * Encode path relative to base_dir for storage in a .gs2. Absolute paths
         * on another volume are left unchanged. Uses forward slashes.
         */
        static std::string make_config_relative(const std::string& base_dir,
                                                const std::string& path);

        static bool is_directory(const std::string& filename);
        static bool ends_with(std::string_view s, std::string_view suffix) noexcept;
        static bool ends_with_icase(std::string_view s, std::string_view suffix) noexcept;
        static std::string get_directory(const std::string& filename);
        static bool is_absolute(const std::string& filename);
};
