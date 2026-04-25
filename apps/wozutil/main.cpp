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

/**
 * wozutil – command-line tool for the gs2_woz library.
 *
 * Usage:
 *   wozutil info   [-v] <file.woz>
 *   wozutil import [-v] [-V <vol>] <input.(do|po|dsk|2mg)> <output.woz>
 *   wozutil save   [-v] <input.woz> <output.woz>
 *
 * Commands:
 *   info    Load a WOZ file and print its INFO chunk, TMAP, and track summary.
 *   import  Convert a block-based disk image to WOZ2 format.
 *   save    Load a WOZ 1.0 or 2.x file and re-save it as WOZ2 (round-trip test).
 *
 * Flags:
 *   -v         Verbose: also print TMAP and per-track bit counts.
 *   -V <num>   DOS 3.3 volume number for import (default: 254 / 0xFE).
 *
 * Testing with CiderPress2:
 *   ~/src/cp2_1.0.5_osx-x64_sc/cp2 <command> <file.woz>
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "util/woz.hpp"
#include "util/media.hpp"

/* Silence the debug_level symbol required by debug.hpp */
uint64_t debug_level = 0;

// ─── Usage ────────────────────────────────────────────────────────────────────

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s info   [-v] <file.woz>\n"
        "  %s import [-v] [-V <vol>] <input.(do|po|dsk|2mg)> <output.woz>\n"
        "  %s save   [-v] <input.woz> <output.woz>\n"
        "\n"
        "  info    Load and display WOZ file metadata\n"
        "  import  Convert a block disk image to WOZ2\n"
        "  save    Load WOZ1 or WOZ2, re-save as WOZ2 (round-trip test)\n"
        "\n"
        "  -v        Verbose: also print TMAP and per-track stats\n"
        "  -V <num>  DOS 3.3 volume number for import (default 254)\n",
        prog, prog, prog);
    exit(1);
}

// ─── Commands ─────────────────────────────────────────────────────────────────

static int cmd_info(const std::string& woz_file, bool verbose) {
    Woz woz;
    if (woz.load(woz_file) != 0) {
        fprintf(stderr, "wozutil: failed to load '%s'\n", woz_file.c_str());
        return 1;
    }

    printf("=== %s ===\n", woz_file.c_str());
    woz.dump_info();

    if (verbose) {
        printf("\n--- TMAP ---\n");
        woz.dump_tmap();
        printf("\n--- Tracks ---\n");
        woz.dump_tracks();
    }
    return 0;
}

static int cmd_import(const std::string& input_file,
                      const std::string& output_file,
                      uint8_t volume,
                      bool verbose) {
    media_descriptor md;
    md.filename = input_file;
    if (identify_media(md) != 0) {
        fprintf(stderr, "wozutil: cannot identify media '%s'\n", input_file.c_str());
        return 1;
    }

    if (verbose) {
        printf("=== Source media: %s ===\n", input_file.c_str());
        display_media_descriptor(md);
    }

    // Override volume number if the user specified one.
    md.dos33_volume = volume;

    Woz woz;
    if (woz.import_from_media(&md) != 0) {
        fprintf(stderr, "wozutil: import failed\n");
        return 1;
    }

    // Tag the output with a brief META entry so tools can identify its origin.
    woz.image().meta["creator"] = "wozutil import";

    if (woz.save(output_file) != 0) {
        fprintf(stderr, "wozutil: save failed\n");
        return 1;
    }

    printf("Imported '%s' → '%s'\n", input_file.c_str(), output_file.c_str());

    if (verbose) {
        printf("\n--- Result ---\n");
        woz.dump_info();
        printf("\n--- TMAP ---\n");
        woz.dump_tmap();
        printf("\n--- Tracks ---\n");
        woz.dump_tracks();
    }
    return 0;
}

static int cmd_save(const std::string& input_woz,
                    const std::string& output_woz,
                    bool verbose) {
    Woz woz;
    if (woz.load(input_woz) != 0) {
        fprintf(stderr, "wozutil: failed to load '%s'\n", input_woz.c_str());
        return 1;
    }

    if (verbose) {
        printf("=== Loaded: %s ===\n", input_woz.c_str());
        woz.dump_info();
    }

    if (woz.save(output_woz) != 0) {
        fprintf(stderr, "wozutil: save failed\n");
        return 1;
    }

    printf("Saved '%s' → '%s'\n", input_woz.c_str(), output_woz.c_str());

    if (verbose) {
        printf("\n--- Tracks ---\n");
        woz.dump_tracks();
    }
    return 0;
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    if (argc < 2) print_usage(argv[0]);

    std::string command = argv[1];
    // Shift so that getopt sees flags starting at argv[1].
    argc -= 1;
    argv += 1;

    bool    verbose = false;
    uint8_t volume  = 254;
    int     opt;

    while ((opt = getopt(argc, argv, "vV:")) != -1) {
        switch (opt) {
            case 'v': verbose = true;  break;
            case 'V': volume = static_cast<uint8_t>(atoi(optarg)); break;
            default:  print_usage(argv[0] - 1); // restore original name
        }
    }

    int remaining = argc - optind;

    if (command == "info") {
        if (remaining < 1) print_usage(argv[0]);
        return cmd_info(argv[optind], verbose);

    } else if (command == "import") {
        if (remaining < 2) print_usage(argv[0]);
        return cmd_import(argv[optind], argv[optind + 1], volume, verbose);

    } else if (command == "save") {
        if (remaining < 2) print_usage(argv[0]);
        return cmd_save(argv[optind], argv[optind + 1], verbose);

    } else {
        fprintf(stderr, "wozutil: unknown command '%s'\n", command.c_str());
        print_usage(argv[0]);
    }
    return 0;
}
