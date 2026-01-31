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

int main(int argc, char *argv[]) {
    FILE *fp;
    unsigned char buffer[8];
    size_t bytes_read;
    int char_index = 0;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <rom_filename>\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "rb");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    // Read 8 bytes at a time (one character)
    while ((bytes_read = fread(buffer, 1, 8, fp)) == 8) {
        // Print character index and file offset
        printf("Character 0x%02X (offset 0x%04X):\n", char_index, char_index * 8);
        
        // Print each line of the character
        for (int line = 0; line < 8; line++) {
            // Process each bit
            for (int bit = 7; bit >= 0; bit--) {
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
