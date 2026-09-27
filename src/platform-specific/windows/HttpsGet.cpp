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

#include "platform-specific/HttpsGet.hpp"
#include "platform-specific/HttpsBody.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::wstring widen(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), n);
    return out;
}

std::string narrow(const wchar_t *text, size_t chars) {
    if (text == nullptr || chars == 0) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(chars), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, static_cast<int>(chars), out.data(), n, nullptr, nullptr);
    return out;
}

std::string trim_token(std::string token) {
    while (!token.empty() && (token.back() == ' ' || token.back() == '\t' || token.back() == '\r'
                              || token.back() == '\n')) {
        token.pop_back();
    }
    size_t begin = 0;
    while (begin < token.size() && (token[begin] == ' ' || token[begin] == '\t')) {
        ++begin;
    }
    token.erase(0, begin);
    if (token.find('\n') != std::string::npos || token.find('\r') != std::string::npos) {
        return {};
    }
    return token;
}

bool query_header(HINTERNET request, DWORD info, const wchar_t *name, std::string& value) {
    DWORD bytes = 0;
    WinHttpQueryHeaders(request, info, name, WINHTTP_NO_OUTPUT_BUFFER, &bytes, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes < sizeof(wchar_t)) {
        return false;
    }
    std::vector<wchar_t> buf(bytes / sizeof(wchar_t) + 1);
    if (!WinHttpQueryHeaders(request, info, name, buf.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    const size_t chars = bytes / sizeof(wchar_t);
    value = narrow(buf.data(), chars);
    return true;
}

}  // namespace

bool https_get_to_file(const std::string& url, const std::string& dest_path, uint64_t max_bytes,
                       const std::atomic<bool> *cancel, HttpsGetResult& out) {
    out = HttpsGetResult{};
    const std::wstring wide_url = widen(url);
    if (wide_url.empty()) {
        out.error = "Pack URL must be https";
        return false;
    }

    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[512];
    wchar_t path[4096];
    wchar_t extra[4096];
    parts.lpszHostName = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = sizeof(extra) / sizeof(extra[0]);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        out.error = "Pack URL must be https";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"GSSquared", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        out.error = "Failed to open HTTP session";
        return false;
    }
    // Resolve, connect, send, receive. Receive stays open so a large pack can finish.
    WinHttpSetTimeouts(session, 10000, 30000, 60000, 0);

    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (connection == nullptr) {
        WinHttpCloseHandle(session);
        out.error = "Failed to connect";
        return false;
    }

    const std::wstring object = std::wstring(path, parts.dwUrlPathLength) + std::wstring(extra, parts.dwExtraInfoLength);
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", object.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
    if (request == nullptr) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        out.error = "Failed to open HTTP request";
        return false;
    }

    bool ok = false;
    HttpsBodyWriter writer;
    if (cancel != nullptr && cancel->load()) {
        out.error = "Download canceled";
    } else if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
               || !WinHttpReceiveResponse(request, nullptr)) {
        out.error = (cancel != nullptr && cancel->load()) ? "Download canceled" : "Download failed";
    } else {
        std::string status_text;
        if (!query_header(request, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, status_text)) {
            out.error = "Download failed";
        } else {
            try {
                out.status = std::stoi(status_text);
            } catch (...) {
                out.status = 0;
            }
            if (out.status != 200) {
                out.error = "Server returned status " + std::to_string(out.status);
            } else {
                std::string length_text;
                if (query_header(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, length_text)) {
                    try {
                        if (std::stoull(length_text) > max_bytes) {
                            out.error = "Pack exceeds 200 MB";
                        }
                    } catch (...) {
                        // Ignore a malformed length. The running byte count still enforces the cap.
                    }
                }
                std::string token;
                if (out.error.empty()
                    && query_header(request, WINHTTP_QUERY_CUSTOM, L"X-GS2-Save-Token", token)) {
                    out.save_token = trim_token(token);
                }
                if (out.error.empty() && !writer.open(dest_path, max_bytes, out.error)) {
                    if (out.error.empty()) {
                        out.error = "Failed to write pack download";
                    }
                }
                if (out.error.empty()) {
                    for (;;) {
                        if (cancel != nullptr && cancel->load()) {
                            out.error = "Download canceled";
                            writer.abort();
                            break;
                        }
                        DWORD available = 0;
                        if (!WinHttpQueryDataAvailable(request, &available)) {
                            out.error = "Download failed";
                            writer.abort();
                            break;
                        }
                        if (available == 0) {
                            break;
                        }
                        std::vector<char> buf(available);
                        DWORD read = 0;
                        if (!WinHttpReadData(request, buf.data(), available, &read)) {
                            out.error = "Download failed";
                            writer.abort();
                            break;
                        }
                        if (read == 0) {
                            break;
                        }
                        if (!writer.write(buf.data(), read, cancel, out.error)) {
                            break;
                        }
                    }
                    if (out.error.empty() && !writer.commit(out.error)) {
                        if (out.error.empty()) {
                            out.error = "Failed to store pack download";
                        }
                    }
                    ok = out.error.empty();
                }
            }
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    out.ok = ok;
    if (!out.ok && out.error.empty()) {
        out.error = "Download failed";
    }
    return out.ok;
}

bool resolve_redirect(const std::wstring& current, const std::string& location, std::wstring& next,
                      std::string& error) {
    if (location.empty()) {
        error = "Redirect left https";
        return false;
    }
    std::wstring wide = widen(location);
    if (location.size() >= 8 && (location.compare(0, 8, "https://") == 0 || location.compare(0, 8, "HTTPS://") == 0)) {
        next = wide;
        return true;
    }
    if (location.size() >= 7 && (location.compare(0, 7, "http://") == 0 || location.compare(0, 7, "HTTP://") == 0)) {
        error = "Redirect left https";
        return false;
    }
    if (location.front() != '/') {
        error = "Redirect left https";
        return false;
    }
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[512];
    parts.lpszHostName = host;
    parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
    if (!WinHttpCrackUrl(current.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
        error = "Redirect left https";
        return false;
    }
    std::wstring url = L"https://" + std::wstring(host, parts.dwHostNameLength);
    if (parts.nPort != INTERNET_DEFAULT_HTTPS_PORT) {
        url += L":" + std::to_wstring(parts.nPort);
    }
    url += wide;
    next = url;
    return true;
}

bool https_put_file(const std::string& url, const std::string& src_path, const std::string& save_token,
                    uint64_t max_bytes, const std::atomic<bool> *cancel, HttpsPutResult& out) {
    out = HttpsPutResult{};
    if (save_token.empty() || save_token.find('\n') != std::string::npos
        || save_token.find('\r') != std::string::npos) {
        out.error = "Save token is not usable";
        return false;
    }
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(src_path, ec);
    if (ec) {
        out.error = "Failed to open pack";
        return false;
    }
    if (file_size > max_bytes) {
        out.error = "Pack exceeds 200 MB";
        return false;
    }
    if (file_size > 0xFFFFFFFFull) {
        out.error = "Pack exceeds 200 MB";
        return false;
    }

    std::wstring current = widen(url);
    if (current.empty()) {
        out.error = "Pack URL must be https";
        return false;
    }

    HINTERNET session = WinHttpOpen(L"GSSquared", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session == nullptr) {
        out.error = "Failed to open HTTP session";
        return false;
    }
    WinHttpSetTimeouts(session, 10000, 30000, 60000, 0);
    const std::wstring headers = L"Content-Type: application/x-tar\r\nX-GS2-Save-Token: " + widen(save_token) + L"\r\n";

    bool finished = false;
    for (int hop = 0; hop < 10 && !finished; ++hop) {
        URL_COMPONENTS parts{};
        parts.dwStructSize = sizeof(parts);
        wchar_t host[512];
        wchar_t path[4096];
        wchar_t extra[4096];
        parts.lpszHostName = host;
        parts.dwHostNameLength = sizeof(host) / sizeof(host[0]);
        parts.lpszUrlPath = path;
        parts.dwUrlPathLength = sizeof(path) / sizeof(path[0]);
        parts.lpszExtraInfo = extra;
        parts.dwExtraInfoLength = sizeof(extra) / sizeof(extra[0]);
        if (!WinHttpCrackUrl(current.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
            out.error = "Pack URL must be https";
            break;
        }
        HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
        if (connection == nullptr) {
            out.error = "Failed to connect";
            break;
        }
        const std::wstring object = std::wstring(path, parts.dwUrlPathLength)
            + std::wstring(extra, parts.dwExtraInfoLength);
        HINTERNET request = WinHttpOpenRequest(connection, L"PUT", object.c_str(), nullptr,
                                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               WINHTTP_FLAG_SECURE);
        if (request == nullptr) {
            WinHttpCloseHandle(connection);
            out.error = "Failed to open HTTP request";
            break;
        }
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

        const DWORD total = static_cast<DWORD>(file_size);
        bool sent = false;
        if (cancel != nullptr && cancel->load()) {
            out.error = "Upload canceled";
        } else if (!WinHttpSendRequest(request, headers.c_str(), static_cast<DWORD>(-1),
                                       WINHTTP_NO_REQUEST_DATA, 0, total, 0)) {
            out.error = "Upload failed";
        } else {
            std::ifstream file(src_path, std::ios::binary);
            std::vector<char> buf(64 * 1024);
            DWORD left = total;
            sent = true;
            while (left > 0 && sent) {
                if (cancel != nullptr && cancel->load()) {
                    out.error = "Upload canceled";
                    sent = false;
                    break;
                }
                const DWORD chunk = left > buf.size() ? static_cast<DWORD>(buf.size()) : left;
                file.read(buf.data(), chunk);
                if (static_cast<DWORD>(file.gcount()) != chunk) {
                    out.error = "Failed to read pack";
                    sent = false;
                    break;
                }
                DWORD wrote = 0;
                if (!WinHttpWriteData(request, buf.data(), chunk, &wrote) || wrote != chunk) {
                    out.error = "Upload failed";
                    sent = false;
                    break;
                }
                left -= chunk;
            }
        }
        if (sent && !WinHttpReceiveResponse(request, nullptr)) {
            out.error = "Upload failed";
            sent = false;
        }
        if (sent) {
            std::string status_text;
            if (!query_header(request, WINHTTP_QUERY_STATUS_CODE, WINHTTP_HEADER_NAME_BY_INDEX, status_text)) {
                out.error = "Upload failed";
            } else {
                try {
                    out.status = std::stoi(status_text);
                } catch (...) {
                    out.status = 0;
                }
                if (out.status == 301 || out.status == 302 || out.status == 307 || out.status == 308) {
                    std::string location;
                    if (!query_header(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location)) {
                        out.error = "Redirect left https";
                    } else {
                        while (!location.empty() && (location.back() == '\0' || location.back() == '\r'
                                                     || location.back() == '\n' || location.back() == ' ')) {
                            location.pop_back();
                        }
                        if (!resolve_redirect(current, location, current, out.error)) {
                            if (out.error.empty()) {
                                out.error = "Redirect left https";
                            }
                        }
                    }
                } else if (out.status == 204) {
                    out.ok = true;
                    finished = true;
                } else {
                    out.error = "Server returned status " + std::to_string(out.status);
                    finished = true;
                }
            }
        }
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        if (!out.error.empty()) {
            break;
        }
    }
    if (!out.ok && !finished && out.error.empty()) {
        out.error = "Upload failed";
    }

    WinHttpCloseHandle(session);
    if (!out.ok && out.error.empty()) {
        out.error = "Upload failed";
    }
    return out.ok;
}
