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

#include <string>

/** `gssquared:https://…/name.gs2pack` launch URLs. The query string is a credential. */
namespace gs2url {

struct PackUrl {
    /** https URL forwarded unchanged, query included, fragment dropped. */
    std::string https_url;
    /** Host, without userinfo or port. Shown in the confirm dialog. */
    std::string host;
    /** Decoded path basename, including `.gs2pack`. Shown in the confirm dialog. */
    std::string filename;
    /** Cache file name. Query is not part of it. */
    std::string cache_name;
};

bool is_gssquared_url(const std::string& text);

/**
 * Accept only `gssquared:https://` whose path ends in `.gs2pack`.
 * `http`, `file`, `gssquared://host/…`, and other documents are rejected.
 */
bool parse_pack_url(const std::string& text, PackUrl& out, std::string& error_out);

/** Machine-local pack cache. Not the extract temp directory. */
std::string pack_cache_directory();

std::string pack_cache_file(const PackUrl& url);

/**
 * A desktop file `%u` field may hand us `file:///path`. Returns the local path.
 * A plain path returns false.
 */
bool decode_file_url(const std::string& text, std::string& path_out);

/**
 * https URL with the query and fragment removed. Empty when `https_url` is not https.
 * This is the Collection PUT target. The launch token stays on the GET only.
 */
std::string save_url_from_https(const std::string& https_url);

/** Write or remove the save-token sidecar next to a cached pack. Empty token removes it and the save-url sidecar. */
bool write_save_token_sidecar(const std::string& pack_path, const std::string& token,
                              std::string& error_out);

/** Write or remove `<pack>.save-url`. Empty url removes it. The url must be https with no query. */
bool write_save_url_sidecar(const std::string& pack_path, const std::string& url,
                            std::string& error_out);

/** Remove `<pack>.save-token` and `<pack>.save-url`. */
void clear_save_sidecars(const std::string& pack_path);

/**
 * Read both sidecars. False when either is missing or not usable.
 * A pack can upload only when both are present.
 */
bool read_save_credentials(const std::string& pack_path, std::string& token_out,
                           std::string& url_out);

}  // namespace gs2url
