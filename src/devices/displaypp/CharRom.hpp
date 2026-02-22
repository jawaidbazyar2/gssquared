#pragma once

#include <cstdint>
#include <cassert>

enum char_mode_t {
    CHAR_MODE_NORMAL,
    CHAR_MODE_INVERSE,
    CHAR_MODE_FLASH,
};

#define MAX_CHAR_SETS 8

struct CharCode {
    uint8_t screen_code;
    char_mode_t mode;
    uint16_t pos; // starting pos in data of char (i.e., char index * 8)
};

struct CharSet {
    CharCode set[256];
};

class CharRom {
    public:
        CharRom(uint8_t *data, int size) {
            this->data = data;
            this->size = size;
            valid = true;
        }

        CharRom(const char *filename);

        ~CharRom();

        // 0 = "main"; 1 = "Alt"; for II+ / IIe.
        // 2,3 - UK
        // 4,5 - etc.
        inline void set_char_set(uint16_t char_set, uint16_t normal_alt) {
            assert(char_set < MAX_CHAR_SETS);
            selected_char_set = (char_set * 2) + normal_alt;
        }

        inline uint8_t get_char_scanline(uint16_t tchar, uint8_t y) {
            uint16_t pos = char_sets[selected_char_set].set[tchar].pos;
            uint16_t index = pos + y;
            uint8_t byte = data[index];
            return byte;
        }

        uint8_t *get_data() {
            return data;
        }

        bool is_valid() {
            return valid;
        }

        inline bool is_flash(uint8_t tchar) {
            return char_sets[selected_char_set].set[tchar].mode == CHAR_MODE_FLASH;
        }

        void print_matrix(uint16_t tchar);

    private:
        uint8_t *data = nullptr;
        int size = 0;
        bool valid = false;
        uint16_t num_char_sets = 1;
        uint16_t selected_char_set = 0;
        CharSet char_sets[2 * MAX_CHAR_SETS]; // up to 8 for Apple IIgs (maybe also later for Apple IIe intl sets)

        inline uint8_t invert_bits(uint8_t b) {
            return ~b;
        }

        uint8_t reverse_bits(uint8_t b);
};