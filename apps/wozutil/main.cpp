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
 *   wozutil export [-v] [-i (do|po)] <input.woz> <output.(do|po|dsk)>
 *
 * Commands:
 *   info    Load a WOZ file and print its INFO chunk, TMAP, and track summary.
 *   import  Convert a block-based disk image to WOZ2 format.
 *   save    Load a WOZ 1.0 or 2.x file and re-save it as WOZ2 (round-trip test).
 *   export  Convert a WOZ file to a 140K block image (.do/.po/.dsk).
 *
 * Flags:
 *   -v         Verbose: also print TMAP and per-track bit counts.
 *   -V <num>   DOS 3.3 volume number for import (default: 254 / 0xFE).
 *   -i do|po   Sector interleave for export (default: inferred from
 *              output extension; .do/.dsk → DO, .po → PO).
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
#include "util/woz_nibblizer.hpp"
#include "util/woz_nibblizer_35.hpp"

/* Silence the debug_level symbol required by debug.hpp */
uint64_t debug_level = 0;

// ─── Usage ────────────────────────────────────────────────────────────────────

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s info   [-v] <file.woz>\n"
        "  %s import [-v] [-V <vol>] <input.(do|po|dsk|2mg)> <output.woz>\n"
        "  %s save   [-v] <input.woz> <output.woz>\n"
        "  %s export [-v] [-i (do|po)] <input.woz> <output.(do|po|dsk)>\n"
        "\n"
        "  info    Load and display WOZ file metadata\n"
        "  import  Convert a block disk image to WOZ2\n"
        "  save    Load WOZ1 or WOZ2, re-save as WOZ2 (round-trip test)\n"
        "  export  Convert WOZ to a 140K block image (.do/.po/.dsk)\n"
        "\n"
        "  -v         Verbose: also print TMAP and per-track stats\n"
        "  -V <num>   DOS 3.3 volume number for import (default 254)\n"
        "  -i do|po   Interleave for export (default: inferred from extension)\n",
        prog, prog, prog, prog);
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
    Woz_Nibblizer *nibblizer = new Woz_Nibblizer_35();
    nibblizer->import_block_image(woz, &md);
    /* if (woz.import_from_media(&md) != 0) {
        fprintf(stderr, "wozutil: import failed\n");
        return 1;
    } */

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

// Returns INTERLEAVE_DO/INTERLEAVE_PO inferred from the output filename's
// extension. .do/.dsk default to DO; .po defaults to PO. Returns
// INTERLEAVE_NONE if the extension is unrecognised.
static media_interleave_t infer_interleave(const std::string& filename) {
    auto dot = filename.find_last_of('.');
    if (dot == std::string::npos) return INTERLEAVE_NONE;
    std::string ext = filename.substr(dot + 1);
    for (auto& c : ext) c = static_cast<char>(tolower(c));
    if (ext == "po")              return INTERLEAVE_PO;
    if (ext == "do" || ext == "dsk") return INTERLEAVE_DO;
    return INTERLEAVE_NONE;
}

static int cmd_export(const std::string& input_woz,
                      const std::string& output_block,
                      media_interleave_t interleave_override,
                      bool verbose) {
    Woz woz;
    if (woz.load(input_woz) != 0) {
        fprintf(stderr, "wozutil: failed to load '%s'\n", input_woz.c_str());
        return 1;
    }

    media_descriptor md;
    md.filename = output_block;
    if (identify_media(md) != 0) {
        fprintf(stderr, "wozutil: cannot identify media '%s'\n", output_block.c_str());
        return 1;
    }
#if 0
    media_interleave_t interleave = interleave_override;
    if (interleave == INTERLEAVE_NONE) {
        interleave = infer_interleave(output_block);
        if (interleave == INTERLEAVE_NONE) {
            fprintf(stderr,
                    "wozutil: cannot infer interleave from '%s'; "
                    "use -i do|po to override\n",
                    output_block.c_str());
            return 1;
        }
    }
#endif

    if (verbose) {
        printf("=== Loaded: %s ===\n", input_woz.c_str());
        woz.dump_info();
/*         printf("Output interleave: %s\n",
               interleave == INTERLEAVE_PO ? "PO (ProDOS)" : "DO (DOS 3.3)"); */
    }

    Woz_Nibblizer *nibblizer = new Woz_Nibblizer_35();
    nibblizer->export_block_image(woz, &md);

#if 0
    disk_image_t out{};
    int rc = woz.export_to_disk_image(out, interleave);
    if (rc != 0) {
        fprintf(stderr,
                "wozutil: export had decode errors (some tracks/sectors did "
                "not yield a clean checksum); writing partial result\n");
    }

    std::string out_copy = output_block; // write_disk_image_po_do_filename takes std::string&
    if (!write_disk_image_po_do_filename(out, out_copy)) {
        fprintf(stderr, "wozutil: write failed\n");
        return 1;
    }

    printf("Exported '%s' → '%s' (%s)\n",
           input_woz.c_str(), output_block.c_str(),
           interleave == INTERLEAVE_PO ? "PO" : "DO");
    return rc == 0 ? 0 : 2; // 2 = wrote partial result
#endif
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

    bool               verbose             = false;
    uint8_t            volume              = 254;
    media_interleave_t interleave_override = INTERLEAVE_NONE;
    int                opt;

    while ((opt = getopt(argc, argv, "vV:i:")) != -1) {
        switch (opt) {
            case 'v': verbose = true;  break;
            case 'V': volume = static_cast<uint8_t>(atoi(optarg)); break;
            case 'i': {
                std::string s = optarg;
                for (auto& c : s) c = static_cast<char>(tolower(c));
                if      (s == "do") interleave_override = INTERLEAVE_DO;
                else if (s == "po") interleave_override = INTERLEAVE_PO;
                else {
                    fprintf(stderr, "wozutil: -i must be 'do' or 'po'\n");
                    print_usage(argv[0] - 1);
                }
                break;
            }
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

    } else if (command == "export") {
        if (remaining < 2) print_usage(argv[0]);
        return cmd_export(argv[optind], argv[optind + 1],
                          interleave_override, verbose);

    } else {
        fprintf(stderr, "wozutil: unknown command '%s'\n", command.c_str());
        print_usage(argv[0]);
    }
    return 0;
}
