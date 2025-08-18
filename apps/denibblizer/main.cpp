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

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "debug.hpp"
#include "devices/diskii/diskii_fmt.hpp"
#include "util/media.hpp"
/* Copy here because I'm too lazy to pull in debug.hpp/cpp from the main tree */
uint64_t debug_level = 0 /* DEBUG_DISKII_FORMAT */;

/**
 * for now, read only!
 */

void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s [-v] [-o output.nib] -d | -p input_file\n", program_name);
    fprintf(stderr, "  -o filename    Write output to filename (default: output.nib)\n");
    fprintf(stderr, "  -v            Verbose mode - dump disk information\n");
    fprintf(stderr, "  -d            DOS 3.3 interleave\n");
    fprintf(stderr, "  -p            ProDOS interleave\n");
    exit(1);
}

int main(int argc, char *argv[]) {
    std::string output_filename = "output.dsk";
    const char* input_filename = nullptr;
    bool verbose = false;
    int opt;
    media_interleave_t interleave = INTERLEAVE_NONE;

    // Process command line options
    while ((opt = getopt(argc, argv, "o:vdp")) != -1) {
        switch (opt) {
            case 'o':
                output_filename = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'd':
                interleave = INTERLEAVE_DO;
                break;
            case 'p':
                interleave = INTERLEAVE_PO;
                break;
            default:
                print_usage(argv[0]);
        }
    }
    if (interleave == INTERLEAVE_NONE) {
        print_usage(argv[0]);
    }

    // Get input filename (first non-option argument)
    if (optind >= argc) {
        print_usage(argv[0]);
    }
    input_filename = argv[optind];

    media_descriptor md;
    md.filename = input_filename;
    identify_media(md);
    display_media_descriptor(md);
    // TODO: make sure it is a nibble image

    nibblized_disk_t nib_disk;
    load_nib_image(nib_disk, input_filename);

    disk_image_t disk_image;

    int ret = denibblize_disk_image(disk_image, nib_disk, interleave);
    if (ret < 0) {
        fprintf(stderr, "Failed to load disk image: %s\n", input_filename);
        exit(1);
    }
    if (verbose) {
        dump_disk_image(disk_image);
    }

    bool ret2 = write_disk_image_po_do_filename(disk_image, output_filename);
    if (!ret2) {
        fprintf(stderr, "Failed to write disk image: %s\n", output_filename.c_str());
        exit(1);
    }

    return 0;
}
