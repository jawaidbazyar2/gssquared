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

#include <stdio.h>
#include <stdlib.h>

#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16

int main(int argc, char *argv[]) {
    FILE *fp;
    unsigned char buffer[CHAR_HEIGHT];
    size_t bytes_read;
    int char_index = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom_filename_stub>\n", argv[0]);
        return 1;
    }

    char rom_filename[256];
    snprintf(rom_filename, sizeof(rom_filename), "../roms/cards/videx/videx-%s.bin", argv[1]);

    fp = fopen(rom_filename, "rb");
    if (!fp) {
        fprintf(stderr, "Error opening file: %s\n", rom_filename);
        return 1;
    }

    // Read 8 bytes at a time (one character)
    while ((bytes_read = fread(buffer, 1, CHAR_HEIGHT, fp)) == CHAR_HEIGHT) {
        // Print character index and file offset
        printf("Character 0x%02X (offset 0x%04X):\n", char_index, char_index * CHAR_HEIGHT);
        
        // Print each line of the character
        for (int line = 0; line < CHAR_HEIGHT; line++) {
            // Process each bit
            for (int bit = CHAR_WIDTH - 1; bit >= 0; bit--) {
                printf("%c", (buffer[line] & (1 << bit)) ? '*' : '.');
            }
            printf("\n");
        }
        printf("\n");  // Blank line between characters
        
        char_index++;
    }

    fclose(fp);
    return 0;
}
