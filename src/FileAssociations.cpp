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

#include "FileAssociations.hpp"

#if defined(__EMSCRIPTEN__) || defined(__APPLE__)

void register_gs2_file_association() {}

#elif defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h>

#include <string>

namespace {

bool read_reg_sz(const wchar_t *subkey, const wchar_t *name, std::wstring& out) {
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    const LSTATUS st = RegGetValueW(HKEY_CURRENT_USER, subkey, name, RRF_RT_REG_SZ,
                                    nullptr, buf, &size);
    if (st != ERROR_SUCCESS) {
        return false;
    }
    out.assign(buf);
    return true;
}

bool write_reg_sz(const wchar_t *subkey, const wchar_t *name, const std::wstring& value) {
    std::wstring current;
    if (read_reg_sz(subkey, name, current) && current == value) {
        return false;
    }
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    RegSetKeyValueW(HKEY_CURRENT_USER, subkey, name, REG_SZ, value.c_str(), bytes);
    return true;
}

std::wstring exe_path() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return {};
    }
    return std::wstring(buf, n);
}

}  // namespace

void register_gs2_file_association() {
    const std::wstring exe = exe_path();
    if (exe.empty()) {
        return;
    }

    const std::wstring quoted = L"\"" + exe + L"\"";
    const std::wstring command = quoted + L" \"%1\"";
    const std::wstring icon = quoted + L",1";

    bool changed = false;
    changed |= write_reg_sz(L"Software\\Classes\\.gs2", nullptr, L"GSSquared.gs2");
    changed |= write_reg_sz(L"Software\\Classes\\.gs2", L"Content Type",
                            L"application/x-gs2-config");
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2", nullptr,
                            L"GS2 System Configuration");
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2\\DefaultIcon", nullptr, icon);
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2\\shell\\open\\command",
                            nullptr, command);

    const std::wstring pack_icon = quoted + L",0";
    changed |= write_reg_sz(L"Software\\Classes\\.gs2pack", nullptr, L"GSSquared.gs2pack");
    changed |= write_reg_sz(L"Software\\Classes\\.gs2pack", L"Content Type",
                            L"application/x-gs2-pack");
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2pack", nullptr, L"GS2 Pack");
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2pack\\DefaultIcon", nullptr,
                            pack_icon);
    changed |= write_reg_sz(L"Software\\Classes\\GSSquared.gs2pack\\shell\\open\\command",
                            nullptr, command);

    if (changed) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
}

#else

#include "paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

namespace {

constexpr const char *kMimeXml =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
    "  <mime-type type=\"application/x-gs2-config\">\n"
    "    <comment>GS2 System Configuration</comment>\n"
    "    <icon name=\"application-x-gs2-config\"/>\n"
    "    <glob pattern=\"*.gs2\"/>\n"
    "  </mime-type>\n"
    "  <mime-type type=\"application/x-gs2-pack\">\n"
    "    <comment>GS2 Pack</comment>\n"
    "    <glob pattern=\"*.gs2pack\"/>\n"
    "  </mime-type>\n"
    "</mime-info>\n";

std::string xdg_data_home() {
    if (const char *xdg = std::getenv("XDG_DATA_HOME"); xdg && xdg[0]) {
        return xdg;
    }
    std::string home;
    Paths::calc_home(home, ".local/share");
    return home;
}

std::string current_executable() {
    if (const char *appimage = std::getenv("APPIMAGE"); appimage && appimage[0]) {
        return appimage;
    }
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        return buf;
    }
    return {};
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool write_if_changed(const std::filesystem::path& path, const std::string& contents) {
    if (read_file(path) == contents) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    const std::filesystem::path tmp = path.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) {
            return false;
        }
        out << contents;
    }
    std::filesystem::rename(tmp, path, ec);
    return !ec;
}

bool copy_if_changed(const std::filesystem::path& src, const std::filesystem::path& dest) {
    if (!std::filesystem::exists(src)) {
        return false;
    }
    if (std::filesystem::exists(dest) && read_file(src) == read_file(dest)) {
        return false;
    }
    std::error_code ec;
    std::filesystem::create_directories(dest.parent_path(), ec);
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}

void run_quiet(const std::string& cmd) {
    (void)std::system((cmd + " >/dev/null 2>&1").c_str());
}

std::string desktop_entry(const std::string& exe) {
    return std::string(
               "[Desktop Entry]\n"
               "Type=Application\n"
               "Version=1.0\n"
               "Name=GSSquared\n"
               "Icon=GSSquared\n"
               "Comment=A complete Apple II series emulator\n"
               "Exec=\"") +
           exe +
           "\" %f\n"
           "GenericName=GSSquared\n"
           "Categories=Game;\n"
           "MimeType=application/x-gs2-config;application/x-gs2-pack;\n"
           "Terminal=false\n";
}

}  // namespace

void register_gs2_file_association() {
    const std::string exe = current_executable();
    if (exe.empty()) {
        return;
    }

    const std::filesystem::path data(xdg_data_home());
    bool changed = false;
    changed |= write_if_changed(data / "applications" / "GSSquared.desktop", desktop_entry(exe));
    changed |= write_if_changed(data / "mime" / "packages" / "gssquared.xml", kMimeXml);

    static const struct {
        const char *resource;
        const char *subdir;
    } icons[] = {
        {"img/mimetype-gs2-16.png", "16x16"},
        {"img/mimetype-gs2-32.png", "32x32"},
        {"img/mimetype-gs2-48.png", "48x48"},
        {"img/mimetype-gs2-128.png", "128x128"},
        {"img/mimetype-gs2-256.png", "256x256"},
    };
    for (const auto& icon : icons) {
        std::string src;
        Paths::calc_base(src, icon.resource);
        const auto dest = data / "icons" / "hicolor" / icon.subdir / "mimetypes" /
                          "application-x-gs2-config.png";
        changed |= copy_if_changed(src, dest);
    }

    if (changed) {
        run_quiet("update-desktop-database \"" + (data / "applications").string() + "\"");
        run_quiet("update-mime-database \"" + (data / "mime").string() + "\"");
    }
    run_quiet("xdg-mime default GSSquared.desktop application/x-gs2-config");
    run_quiet("xdg-mime default GSSquared.desktop application/x-gs2-pack");
}

#endif
