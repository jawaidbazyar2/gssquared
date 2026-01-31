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

#include <iostream>
#include <cstdio>

#include "gs2.hpp"
#include "cpu.hpp"
#include "loader.hpp"

char *loader_filename = NULL;
uint16_t loader_address = 0x0000;

void loader_set_file_info(char *filename, uint16_t address) {
    loader_filename = filename;
    loader_address = address;
}

void loader_execute(computer_t *computer) {

    if (loader_filename == NULL) {
        fprintf(stderr, "Loader filename not set\n");
        return;
    }

    FILE* file = fopen(loader_filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file %s\n", loader_filename);
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Read file into buffer
    uint8_t *memory_chunk = (uint8_t *)malloc(file_size);
    if (memory_chunk == NULL) {
        fprintf(stderr, "Failed to allocate memory for file %s\n", loader_filename);
        fclose(file);
        return;
    }

    fread(memory_chunk, 1, file_size, file);
    fclose(file);

    // Load into RAM at loader_address
    for (long i = 0; i < file_size; i++) {
        computer->cpu->mmu->write(loader_address + i, memory_chunk[i]);
    }

    free(memory_chunk);
}