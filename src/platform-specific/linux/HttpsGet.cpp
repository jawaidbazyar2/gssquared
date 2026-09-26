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

#include <curl/curl.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

namespace {

struct CurlDownload {
    HttpsBodyWriter *writer = nullptr;
    const std::atomic<bool> *cancel = nullptr;
    std::string dest;
    uint64_t max_bytes = 0;
    long content_length = -1;
    int status = 0;
    bool saw_status = false;
    bool opened = false;
    bool failed = false;
    std::string save_token;
    std::string error;
};

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

bool header_name_is(const char *line, size_t len, const char *name) {
    const size_t name_len = std::strlen(name);
    if (len < name_len + 1) {
        return false;
    }
    for (size_t i = 0; i < name_len; ++i) {
        char c = line[i];
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != name[i]) {
            return false;
        }
    }
    return line[name_len] == ':';
}

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    auto *download = static_cast<CurlDownload *>(userdata);
    const size_t len = size * nitems;
    if (download->cancel != nullptr && download->cancel->load()) {
        download->error = "Download canceled";
        download->failed = true;
        return 0;
    }
    if (len >= 5 && std::strncmp(buffer, "HTTP/", 5) == 0) {
        download->saw_status = true;
        download->content_length = -1;
        download->save_token.clear();
        download->status = 0;
        const char *space = static_cast<const char *>(std::memchr(buffer, ' ', len));
        if (space != nullptr && space + 1 < buffer + len) {
            download->status = std::atoi(space + 1);
        }
        return len;
    }
    if (header_name_is(buffer, len, "content-length")) {
        const char *value = static_cast<const char *>(std::memchr(buffer, ':', len));
        if (value != nullptr) {
            download->content_length = std::atol(value + 1);
        }
        return len;
    }
    if (header_name_is(buffer, len, "x-gs2-save-token")) {
        const char *value = static_cast<const char *>(std::memchr(buffer, ':', len));
        if (value != nullptr) {
            download->save_token = trim_token(std::string(value + 1, buffer + len - (value + 1)));
        }
        return len;
    }
    const bool blank = (len == 2 && buffer[0] == '\r' && buffer[1] == '\n') || (len == 1 && buffer[0] == '\n');
    if (blank && download->saw_status && download->status == 200
        && download->content_length > static_cast<long>(download->max_bytes)) {
        download->error = "Pack exceeds 200 MB";
        download->failed = true;
        return 0;
    }
    return len;
}

size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *download = static_cast<CurlDownload *>(userdata);
    if (download->failed) {
        return 0;
    }
    if (download->status != 200) {
        download->error = "Server returned status " + std::to_string(download->status);
        download->failed = true;
        return 0;
    }
    if (!download->opened) {
        if (!download->writer->open(download->dest, download->max_bytes, download->error)) {
            download->failed = true;
            return 0;
        }
        download->opened = true;
    }
    const size_t n = size * nmemb;
    if (!download->writer->write(ptr, n, download->cancel, download->error)) {
        download->failed = true;
        return 0;
    }
    return n;
}

int xfer_cb(void *userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto *download = static_cast<CurlDownload *>(userdata);
    if (download->cancel != nullptr && download->cancel->load()) {
        download->error = "Download canceled";
        download->failed = true;
        return 1;
    }
    return 0;
}

}  // namespace

bool https_get_to_file(const std::string& url, const std::string& dest_path, uint64_t max_bytes,
                       const std::atomic<bool> *cancel, HttpsGetResult& out) {
    out = HttpsGetResult{};
    static std::once_flag curl_once;
    std::call_once(curl_once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    if (url.compare(0, 8, "https://") != 0 && url.compare(0, 8, "HTTPS://") != 0) {
        out.error = "Pack URL must be https";
        return false;
    }

    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        out.error = "Failed to start download";
        return false;
    }

    HttpsBodyWriter writer;
    CurlDownload download;
    download.writer = &writer;
    download.cancel = cancel;
    download.dest = dest_path;
    download.max_bytes = max_bytes;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "GSSquared");
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &download);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xfer_cb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &download);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif

    const CURLcode rc = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    out.status = static_cast<int>(status != 0 ? status : download.status);
    out.save_token = download.save_token;

    if (download.opened && (rc != CURLE_OK || download.failed || out.status != 200)) {
        writer.abort();
        download.opened = false;
    }
    if (rc == CURLE_OK && !download.failed && out.status == 200) {
        if (!download.opened && !writer.open(dest_path, max_bytes, download.error)) {
            download.failed = true;
        } else if (!writer.commit(download.error)) {
            download.failed = true;
        }
    }
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK && download.error.empty()) {
        download.error = curl_easy_strerror(rc);
        download.failed = true;
    }
    if (out.status != 200 && download.error.empty()) {
        download.error = "Server returned status " + std::to_string(out.status);
        download.failed = true;
    }
    out.error = download.error;
    out.ok = rc == CURLE_OK && !download.failed && out.status == 200 && download.error.empty();
    if (!out.ok && out.error.empty()) {
        out.error = "Download failed";
    }
    return out.ok;
}
