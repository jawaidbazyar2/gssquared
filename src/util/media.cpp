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

/* Functions to handle setting up media descriptors */
#include <filesystem>
#include <iostream>
#include <sys/stat.h>

#include "gs2.hpp"
#include "cpu.hpp"
#include "media.hpp"
#include "debug.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "devices/diskii/diskii.hpp"
#include "strndup.h"

/**
 * First goal:
 * generic routine. Given a filename, create a media descriptor struct that informs
 * device drivers how to access the media.
 */

/**
 * Identify file type. Given a filename, inspect the file and return a media_descriptor
 * 
 * The filename is the only thing that is passed in in the md.
 */

/* int compare_suffix(const char *filename, const char *suffix) {
    int i = strlen(filename);
    int j = strlen(suffix);
    if (i < (j+1)) return 0;
    //printf("Comparing %s with %s\n", filename + i - j, suffix);
    return strncasecmp(filename + i - j, suffix, j) == 0;
}
 */
bool compare_suffix(const std::string& filename, const char* suffix) {
    size_t filename_length = filename.length();
    size_t suffix_length = strlen(suffix);
    
    if (filename_length < suffix_length) {
        return false;
    }
    
    // Get the substring of filename that should match the suffix
    std::string file_suffix = filename.substr(filename_length - suffix_length);
    
    // Compare case-insensitively
    // Note: Using platform-independent approach for case-insensitive comparison
    return std::equal(
        file_suffix.begin(), file_suffix.end(),
        suffix, suffix + suffix_length,
        [](char a, char b) { return std::tolower(a) == std::tolower(b); }
    );
}

int get_file_size(const std::string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) return -1;
    return st.st_size;
}

static inline uint32_t le32_to_cpu(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24);
}

static inline uint16_t le16_to_cpu(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8);
}

int read_2mg_header(format_2mg_t &hdr_out, const std::string& filename) {
    format_2mg_raw_t raw;
    
    FILE* fp = fopen(filename.c_str(), "rb");
    if (!fp) return -1;
    
    if (fread(&raw, sizeof(format_2mg_raw_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    
    // Verify magic number "2IMG"
    if (memcmp(raw.id, "2IMG", 4) != 0) {
        return -1;
    }
    
    // Copy fixed-size fields
    memcpy(hdr_out.id, raw.id, 4);
    hdr_out.id[4] = 0;          // null terminate
    memcpy(hdr_out.creator, raw.creator, 4);
    hdr_out.creator[4] = 0;     // null terminate
    
    // Convert little-endian fields
    hdr_out.header_size = le16_to_cpu(raw.header_size);
    hdr_out.version = le16_to_cpu(raw.version);
    hdr_out.flag = le32_to_cpu(raw.image_format);
    hdr_out.block_count = le32_to_cpu(raw.block_count);
    hdr_out.bytes_count = le32_to_cpu(raw.data_length);
    hdr_out.comment_offset = le32_to_cpu(raw.comment_offset);
    hdr_out.comment_length = le32_to_cpu(raw.comment_length);
    hdr_out.creator_data = le32_to_cpu(raw.creator_data);
    hdr_out.creator_data_length = le32_to_cpu(raw.creator_data_length);
    hdr_out.image_format = le32_to_cpu(raw.image_format);

    if (hdr_out.comment_offset != 0 && hdr_out.comment_length > 0) {
        fseek(fp, hdr_out.comment_offset, SEEK_SET);
        hdr_out.comment_content = new char[hdr_out.comment_length + 1];
        fread(hdr_out.comment_content, 1, hdr_out.comment_length, fp);
        hdr_out.comment_content[hdr_out.comment_length] = 0;
    } else hdr_out.comment_content = nullptr;

    if (hdr_out.creator_data != 0 && hdr_out.creator_data_length > 0) {
        fseek(fp, hdr_out.creator_data, SEEK_SET);
        hdr_out.creator_data_content = new char[hdr_out.creator_data_length + 1];
        fread(hdr_out.creator_data_content, 1, hdr_out.creator_data_length, fp);
        hdr_out.creator_data_content[hdr_out.creator_data_length] = 0;
    } else hdr_out.creator_data_content = nullptr;

    fclose(fp);

    return 0;
}

format_2mg_creator_data_t format_2mg_creator_list[] = {
    {"!nfc", "Asimov"},
    {"B2TR", "Bernie ][ the Rescue"},
    {"CTKG", "Catakig"},
    {"ShIm", "Sheppy's ImageMaker"},
    {"WOOF", "Sweet 16"},
    {"XGS!", "XGS"},
    {"CdrP", "CiderPress"}
};

const int num_2mg_creators = (sizeof(format_2mg_creator_list) / sizeof(format_2mg_creator_data_t));

const char *get_2mg_creator_name(char *creator_id) {
    for (int i = 0; i < num_2mg_creators; i++) {
        if (strncmp(format_2mg_creator_list[i].creator, creator_id, 4) == 0) {
            return format_2mg_creator_list[i].name;
        }
    }
    return "(Unknown)";
}


const char * get_media_type_name(media_type_t media_type) {
    switch (media_type) {
        case MEDIA_NYBBLE: return "NYBBLE";
        case MEDIA_PRENYBBLE: return "PRE-NYBBLE";
        case MEDIA_BLK: return "BLOCK";
        default: return "UNKNOWN";
    }
}

const char * get_interleave_name(media_interleave_t interleave) {
    switch (interleave) {
        case INTERLEAVE_NONE: return "NONE";
        case INTERLEAVE_DO: return "DOS";
        case INTERLEAVE_PO: return "PRODOS";
        case INTERLEAVE_CPM: return "CP/M";
        default: return "UNKNOWN";
    }
}

int display_media_descriptor(media_descriptor& md) {
    std::cout << "<> Media Descriptor: " << md.filename << std::endl;
    std::cout << "  Media Type: " << get_media_type_name(md.media_type) << std::endl;
    std::cout << "  Interleave: " << get_interleave_name(md.interleave) << std::endl;
    std::cout << "  Block Size: " << md.block_size << std::endl;
    std::cout << "  Block Count: " << md.block_count << std::endl;
    std::cout << "  File Size: " << md.file_size << std::endl;
    std::cout << "  Data Size: " << md.data_size << std::endl;
    std::cout << "  Data Offset: " << md.data_offset << std::endl;
    std::cout << "  Write Protected: " << (md.write_protected ? "Yes" : "No") << std::endl;
    std::cout << "  DOS 3.3 Volume: " << md.dos33_volume << std::endl;
    return 0;
}

int display_2mg_header(format_2mg_t& hdr) {
    std::cout << "<> 2MG Header: " << std::endl;
    std::cout << "  ID: " << hdr.id << std::endl;
    std::cout << "  Creator: " << hdr.creator << " (" << get_2mg_creator_name(hdr.creator) << ")" << std::endl;
    std::cout << "  Header Size: " << hdr.header_size << std::endl;
    std::cout << "  Version: " << hdr.version << std::endl;
    std::cout << "  Image Format: " << hdr.flag << std::endl;
    std::cout << "  Block Count: " << hdr.block_count << std::endl;
    std::cout << "  Bytes Count: " << hdr.bytes_count << std::endl;
    std::cout << "  Comment Offset: " << hdr.comment_offset << std::endl;
    std::cout << "  Comment Length: " << hdr.comment_length << std::endl;
    if (hdr.comment_content && hdr.comment_length > 0) {
        std::cout << "  Comment Content: " << std::endl << "---" << std::endl;
        std::cout << hdr.comment_content;
        std::cout << std::endl << "---" << std::endl;
    }
    std::cout << "  Creator Data: " << hdr.creator_data << std::endl;
    std::cout << "  Creator Data Length: " << hdr.creator_data_length << std::endl;
    if (hdr.creator_data_content && hdr.creator_data_length > 0) {
        std::cout << "  Creator Data Content: " << std::endl << "---" << std::endl;
        std::cout << hdr.creator_data_content;
        std::cout << std::endl << "---" << std::endl;
    }
    return 0;
}

/**
 * Extracts the filename portion from a full pathname.
 * 
 * @param pathname The full path to extract the filename from
 * @return A newly allocated string containing just the filename portion
 *         (caller is responsible for freeing this memory)
 */
/* char* extract_filename(const char* pathname) {
    if (!pathname) {
        return nullptr;
    }
    
    // Find the last directory separator (handle both Unix and Windows paths)
    const char* lastSlash = strrchr(pathname, '/');
    const char* lastBackslash = strrchr(pathname, '\\');
    
    // Determine which separator was found last (if any)
    const char* lastSeparator = nullptr;
    if (lastSlash && lastBackslash) {
        lastSeparator = (lastSlash > lastBackslash) ? lastSlash : lastBackslash;
    } else if (lastSlash) {
        lastSeparator = lastSlash;
    } else {
        lastSeparator = lastBackslash;
    }
    
    // If no separator was found, the entire string is the filename
    const char* filename = lastSeparator ? lastSeparator + 1 : pathname;
    
    // Allocate memory for the new string and copy the filename
    char* result = strndup(filename, 18); // limit to 16 characters.
    return result;
} */

std::string extract_filename(const std::string& pathname) {
    std::filesystem::path p(pathname);
    return p.filename().string();
}

namespace fs = std::filesystem;

bool isFileReadOnly(const std::string& filePath) {
    // First check if the file exists
    if (!fs::exists(filePath)) {
        return false; // File doesn't exist, so not read-only
    }
    
    // Check if we have write permissions by attempting to open it for writing
    // but don't truncate it (ios::ate positions at the end without truncating)
    std::error_code ec;
    fs::file_status status = fs::status(filePath, ec);
    
    if (ec) {
        std::cerr << "Error accessing file: " << ec.message() << std::endl;
        return true; // Assume read-only if there's an error
    }
    
    // Check permissions via filesystem
    fs::perms p = status.permissions();
    
    // Cross-platform approach
    #ifdef _WIN32
        // On Windows
        return (p & fs::perms::owner_write) == fs::perms::none;
    #else
        // On Unix-like systems, check user write permission
        return (p & fs::perms::owner_write) == fs::perms::none;
    #endif
}

int identify_media(media_descriptor& md) {
    if (isFileReadOnly(md.filename)) md.write_protected = true;
    else md.write_protected = false;
    if (compare_suffix(md.filename, ".2mg")) {
        format_2mg_t hdr;
        if (read_2mg_header(hdr, md.filename) != 0) {
            std::cerr << "Failed to read 2MG header: " << md.filename << std::endl;
            return -1;
        }
        display_2mg_header(hdr);

        if (hdr.image_format == 0x00000000) { // DOS 3.3 Sector Order. Only ever 143k disks.
            md.interleave = INTERLEAVE_DO;
            md.media_type = MEDIA_NYBBLE;
        } else if (hdr.image_format == 0x00000001) { // ProDOS Sector Order.
            // if disk size is 143k, treat as a nybble disk.
            if (hdr.bytes_count == 560 * 256) { // floppy
                md.interleave = INTERLEAVE_PO;
                md.media_type = MEDIA_NYBBLE;
            } else {                            // any other media size, just a bunch of blocks.
                md.interleave = INTERLEAVE_NONE;
                md.media_type = MEDIA_BLK;
            }
        } else if (hdr.image_format == 0x00000002) { // NIB data
            md.interleave = INTERLEAVE_NONE;
            md.media_type = MEDIA_PRENYBBLE;
        } else {
            std::cerr << "Unknown image format: " << hdr.image_format << std::endl;
            return -1;
        }
        md.file_size = get_file_size(md.filename);
        md.block_count = hdr.block_count;
        md.block_size = hdr.bytes_count / hdr.block_count;
        md.data_size = hdr.bytes_count;
        md.data_offset = hdr.header_size;
        md.write_protected = (hdr.flag & FLAG_LOCKED) != 0; // use WP flag in .2mg file.
        md.dos33_volume = (hdr.flag & FLAG_DOS33) != 0 ? (hdr.flag & FLAG_DOS33_VOL_MASK) : 254; // if not set, then 254

    } else if (compare_suffix(md.filename, ".hdv")) {
        md.media_type = MEDIA_BLK;
        // get size of file on disk
        md.file_size = get_file_size(md.filename);
        md.block_size = 512;
        md.block_count = md.file_size / md.block_size;
        md.data_size = md.file_size;
        md.interleave = INTERLEAVE_NONE;
        md.data_offset = 0;
    } else if (compare_suffix(md.filename, ".do") || compare_suffix(md.filename, ".dsk")) {
        md.media_type = MEDIA_NYBBLE;
        // if file size is not 140K, then error.
        md.file_size = get_file_size(md.filename);
        if (md.file_size != 560 * 256) {
            std::cerr << "File size is not 140K: " << md.filename << std::endl;
            return -1;
        }
        md.block_size = 256;
        md.block_count = md.file_size / md.block_size;
        md.data_size = md.file_size;
        md.interleave = INTERLEAVE_DO;
        md.data_offset = 0;
        //md.write_protected = false /*true*/;
        md.dos33_volume = 254; // might want to try to snag this from the DOS33 VTOC
    } else if (compare_suffix(md.filename, ".po")) {
        md.media_type = MEDIA_NYBBLE;
        // if file size is 140K, it's a 5.25 image.
        md.file_size = get_file_size(md.filename);
        if (md.file_size == 560 * 256) {
            md.media_type = MEDIA_NYBBLE;
            md.block_size = 256;
        md.block_size = 256;        } else {
            md.media_type = MEDIA_BLK;
            md.block_size = 512;
        }
        md.block_count = md.file_size / md.block_size;
        md.data_size = md.file_size;
        md.interleave = INTERLEAVE_PO;
        md.data_offset = 0;
        //md.write_protected = false /*true*/;
        md.dos33_volume = 0x01; // might want to try to snag this from the DOS33 VTOC
    } else if (compare_suffix(md.filename, ".nib")) {
        md.media_type = MEDIA_PRENYBBLE;
        md.file_size = get_file_size(md.filename);
        md.data_size = 140 * 1024;
        md.block_size = 256;
        md.block_count = 560; // assumed 560 sectors on a 143K diskette.
        md.interleave = INTERLEAVE_NONE;
        md.data_offset = 0;
    } else {
        std::cerr << "Unknown media type: " << md.filename << std::endl;
        return -1;
    }
    md.filestub = extract_filename(md.filename);
    return 0;
}
