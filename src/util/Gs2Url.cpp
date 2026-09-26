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

#include "util/Gs2Url.hpp"

#include "paths.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace gs2url {
namespace {

bool starts_with_icase(const std::string& text, const char *prefix) {
    const size_t n = std::char_traits<char>::length(prefix);
    if (text.size() < n) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        const char a = text[i];
        const char b = prefix[i];
        const char al = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
        const char bl = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
        if (al != bl) {
            return false;
        }
    }
    return true;
}

std::string to_lower(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool percent_decode(const std::string& in, std::string& out, std::string& error_out) {
    out.clear();
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] != '%') {
            out.push_back(in[i]);
            continue;
        }
        if (i + 2 >= in.size()) {
            error_out = "Bad percent-encoding in pack URL";
            return false;
        }
        const int hi = hex_value(in[i + 1]);
        const int lo = hex_value(in[i + 2]);
        if (hi < 0 || lo < 0) {
            error_out = "Bad percent-encoding in pack URL";
            return false;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
    }
    return true;
}

uint32_t fnv1a(const std::string& text) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : text) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

std::string sanitize_stem(std::string stem) {
    if (stem.size() > 80) {
        stem.resize(80);
    }
    for (char& c : stem) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '.' || c == '_' || c == '-';
        if (!ok) {
            c = '_';
        }
    }
    while (!stem.empty() && (stem.back() == '.' || stem.back() == ' ' || stem.back() == '_')) {
        stem.pop_back();
    }
    if (stem.empty() || stem == "." || stem == "..") {
        return "pack";
    }
    return stem;
}

std::string cache_name_for(const std::string& host, const std::string& decoded_path,
                           const std::string& filename) {
    std::string stem = filename;
    if (Paths::ends_with_icase(stem, ".gs2pack")) {
        stem.resize(stem.size() - 8);
    }
    stem = sanitize_stem(stem);
    const uint32_t hash = fnv1a(to_lower(host) + "\n" + decoded_path);
    std::ostringstream out;
    out << stem << '-' << std::hex << std::setw(8) << std::setfill('0') << hash << ".gs2pack";
    return out.str();
}

}  // namespace

bool is_gssquared_url(const std::string& text) {
    return starts_with_icase(text, "gssquared:");
}

bool parse_pack_url(const std::string& text, PackUrl& out, std::string& error_out) {
    if (!is_gssquared_url(text)) {
        error_out = "Not a gssquared: URL";
        return false;
    }
    const std::string rest = text.substr(std::char_traits<char>::length("gssquared:"));
    if (!starts_with_icase(rest, "https://")) {
        error_out = "Pack URL must be gssquared:https://…";
        return false;
    }
    const std::string https = "https://" + rest.substr(std::char_traits<char>::length("https://"));
    const size_t scheme = std::char_traits<char>::length("https://");
    const size_t cut = https.find_first_of("/?#", scheme);
    const std::string authority = https.substr(scheme, cut == std::string::npos ? std::string::npos : cut - scheme);
    if (authority.empty() || authority.find('@') != std::string::npos) {
        error_out = "Pack URL host is missing";
        return false;
    }
    std::string host = authority;
    if (host.front() == '[') {
        const auto end = host.find(']');
        if (end == std::string::npos) {
            error_out = "Pack URL host is missing";
            return false;
        }
        host = host.substr(1, end - 1);
    } else {
        const auto colon = host.rfind(':');
        if (colon != std::string::npos) {
            host = host.substr(0, colon);
        }
    }
    if (host.empty()) {
        error_out = "Pack URL host is missing";
        return false;
    }

    std::string path_and_more = (cut == std::string::npos) ? std::string() : https.substr(cut);
    if (path_and_more.empty() || path_and_more.front() != '/') {
        error_out = "Pack URL is not a .gs2pack";
        return false;
    }
    const size_t q = path_and_more.find_first_of("?#");
    const std::string raw_path = path_and_more.substr(0, q);
    std::string query;
    if (q != std::string::npos && path_and_more[q] == '?') {
        const size_t hash = path_and_more.find('#', q);
        query = path_and_more.substr(q, hash == std::string::npos ? std::string::npos : hash - q);
    }
    std::string path;
    if (!percent_decode(raw_path, path, error_out)) {
        return false;
    }
    if (!Paths::ends_with_icase(path, ".gs2pack")) {
        error_out = "Pack URL is not a .gs2pack";
        return false;
    }
    const auto slash = path.rfind('/');
    const std::string filename = (slash == std::string::npos) ? path : path.substr(slash + 1);
    if (filename.empty() || filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos
        || filename == "." || filename == "..") {
        error_out = "Pack URL filename is not usable";
        return false;
    }

    PackUrl parsed;
    parsed.https_url = https.substr(0, scheme) + authority + raw_path + query;
    parsed.host = host;
    parsed.filename = filename;
    parsed.cache_name = cache_name_for(host, path, filename);
    out = std::move(parsed);
    return true;
}

std::string pack_cache_directory() {
#if defined(__APPLE__)
    const char *home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        return {};
    }
    return std::string(home) + "/Library/Caches/GSSquared/packs";
#elif defined(_WIN32)
    const char *local = std::getenv("LOCALAPPDATA");
    if (local != nullptr && local[0] != '\0') {
        return std::string(local) + "\\GSSquared\\packs";
    }
    const char *profile = std::getenv("USERPROFILE");
    if (profile != nullptr && profile[0] != '\0') {
        return std::string(profile) + "\\AppData\\Local\\GSSquared\\packs";
    }
    return {};
#else
    if (const char *xdg = std::getenv("XDG_CACHE_HOME"); xdg != nullptr && xdg[0] != '\0') {
        return std::string(xdg) + "/gssquared/packs";
    }
    const char *home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') {
        return {};
    }
    return std::string(home) + "/.cache/gssquared/packs";
#endif
}

std::string pack_cache_file(const PackUrl& url) {
    const std::string dir = pack_cache_directory();
    if (dir.empty()) {
        return {};
    }
    return (std::filesystem::path(dir) / url.cache_name).string();
}

bool decode_file_url(const std::string& text, std::string& path_out) {
    if (!starts_with_icase(text, "file:")) {
        return false;
    }
    std::string rest = text.substr(5);
    if (starts_with_icase(rest, "//localhost/")) {
        rest = rest.substr(std::char_traits<char>::length("//localhost"));
    } else if (rest.size() >= 2 && rest[0] == '/' && rest[1] == '/') {
        rest = rest.substr(2);
        if (!rest.empty() && rest.front() != '/') {
            return false;
        }
    }
    std::string error;
    std::string decoded;
    if (!percent_decode(rest, decoded, error) || decoded.empty()) {
        return false;
    }
#if defined(_WIN32)
    if (decoded.size() >= 3 && decoded[0] == '/' && decoded[2] == ':') {
        decoded.erase(decoded.begin());
    }
#endif
    path_out = decoded;
    return true;
}

bool write_save_token_sidecar(const std::string& pack_path, const std::string& token,
                              std::string& error_out) {
    const std::filesystem::path sidecar = pack_path + ".save-token";
    std::error_code ec;
    if (token.empty()) {
        std::filesystem::remove(sidecar, ec);
        return true;
    }
    if (token.find('\n') != std::string::npos || token.find('\r') != std::string::npos
        || token.size() > 4096) {
        error_out = "Save token is not usable";
        return false;
    }
    std::ofstream out(sidecar, std::ios::binary | std::ios::trunc);
    if (!out) {
        error_out = "Failed to write save token";
        return false;
    }
    out << token;
    out.close();
    if (!out) {
        error_out = "Failed to write save token";
        return false;
    }
#if !defined(_WIN32)
    chmod(sidecar.c_str(), 0600);
#endif
    return true;
}

}  // namespace gs2url
