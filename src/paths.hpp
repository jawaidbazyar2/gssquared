/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <string>

const std::string& get_base_path(bool console_mode);
const std::string& get_pref_path(void);


class Paths {
    static std::string base_path;
    static std::string pref_path;
    static std::string docs_folder;
    static std::string home_folder;

    public:
        static void initialize(bool console_mode);
        static void calc_base(std::string& return_path, std::string file) ;
        static void calc_pref(std::string& return_path, std::string file) ;
        static void calc_docs(std::string& return_path, std::string file) ;
        static void calc_home(std::string& return_path, std::string file) ;
};