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

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <unistd.h>

#include "util/media.hpp"
#include "util/woz.hpp"

/* Required by gs2_devices_diskii_fmt (pulled in transitively via gs2_woz). */
uint64_t debug_level = 0;

static void print_usage(const char* prog) {
    fprintf(stderr, "Usage: %s [-v] <filename>\n", prog);
    fprintf(stderr, "  -v   Verbose: for WOZ files, also print TMAP and per-track stats\n");
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    int opt;

    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v': verbose = true; break;
            default:  print_usage(argv[0]); return 1;
        }
    }

    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }

    media_descriptor md;
    md.filename = argv[optind];

    if (identify_media(md) != 0) {
        std::cerr << "Failed to identify media: " << md.filename << std::endl;
        return 1;
    }
    display_media_descriptor(md);

    if (md.media_type == MEDIA_WOZ) {
        Woz woz;
        if (woz.load(md.filename) == 0) {
            printf("\n--- WOZ detail ---\n");
            woz.dump_info();
            if (verbose) {
                printf("\n");
                woz.dump_tmap();
                printf("\n");
                woz.dump_tracks();
            }
        } else {
            std::cerr << "Warning: could not parse WOZ chunks in " << md.filename << "\n";
        }
    }

    return 0;
}
