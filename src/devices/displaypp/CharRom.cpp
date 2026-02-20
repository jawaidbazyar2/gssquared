#include <cstdint>
#include <cstdio>
#include <new>

#include "paths.hpp"
#include "CharRom.hpp"


CharRom::CharRom(const char *filename) {
    data = new(std::align_val_t(64)) uint8_t[16386]; // max char rom size

    std::string fullfilename;
    fullfilename.assign(get_base_path(false));
    fullfilename.append(filename);

    FILE *f = fopen(fullfilename.c_str(), "rb");
    if (!f) {
        printf("Failed to open char rom file: %s\n", fullfilename.c_str());
        valid = false;
        ::operator delete[](data, std::align_val_t(64));
        return;
    }

    size = fread(data, 1, 16384, f);
    fclose(f);
    if (size != 2048 && size != 4096 && size != 8192 && size != 16384) {
        printf("Invalid char rom file: %s\n", filename);
        valid = false;
        ::operator delete[](data, std::align_val_t(64));
        return;
    }

    // if size == 2048, it's an Apple II Plus Char ROM, and we need to reverse AND 
    // also invert bits for the inverse characters.
    if (size == 2048) {
        for (uint16_t i = 0; i < 256; i++) {
            char_mode_t cmode;

            num_char_sets=1; // TODO: international ROMs may have more than 1.
            char_sets[0].set[i].screen_code = i;
            if ((i & 0xC0) == 0x40) cmode = CHAR_MODE_FLASH;
            else if (i < 0x80) cmode = CHAR_MODE_INVERSE;
            else cmode = CHAR_MODE_NORMAL;
            char_sets[0].set[i].mode = cmode;
            char_sets[0].set[i].pos = i * 8;
        }
        for (int i = 0; i < 2048; i++) {
            data[i] = reverse_bits(data[i]);
            if (i < (0x40 * 8)) { // invert chars 0x00 - 0x3F.
                data[i] = invert_bits(data[i]);
            }
        }
    } else { // iie and on roms, we need to invert the bits
        // iie - 0-255 are alt char set, 256-511 are graphics, normal char set is constructed from subset of alt.
        // need to create two character sets.
        uint16_t num_char_sets = 1;
        if (size == 16384) num_char_sets = 16;
        else num_char_sets = 2;
        for (uint16_t char_set = 0; char_set < num_char_sets; char_set+=2) {
            for (uint16_t i = 0; i < 256; i++) {
                char_mode_t cmode;
                uint16_t char_index = i + ((char_set/2) * 256);

                char_sets[char_set].set[i].screen_code = char_index;
                char_sets[char_set+1].set[i].screen_code = char_index;

                // Normal character set
                if ((i & 0xC0) == 0x40) cmode = CHAR_MODE_FLASH;
                else if (i < 0x80) cmode = CHAR_MODE_INVERSE;
                else cmode = CHAR_MODE_NORMAL;
                if (cmode == CHAR_MODE_FLASH) {
                    char_sets[char_set].set[i].pos = (char_index - 0x40) * 8; // put flash down there.
                } else {
                    char_sets[char_set].set[i].pos = char_index * 8;
                }
                char_sets[char_set].set[i].mode = cmode;

                // Alternate character set
                if (i & 0x80) cmode = CHAR_MODE_NORMAL;
                else cmode = CHAR_MODE_INVERSE;
                char_sets[char_set+1].set[i].pos = char_index * 8;
                char_sets[char_set+1].set[i].mode = cmode;
            }

        }
        // TODO: don't invert GS. I need to invert the rom file bits
        for (int i = 0; i < size; i++) {
            data[i] = invert_bits(data[i]);
        }
    }
    valid = true;
}

CharRom::~CharRom() {
    ::operator delete[](data, std::align_val_t(64));
}

uint8_t CharRom::reverse_bits(uint8_t b) {
    // in apple ii+, the high bit set on a character line means it's a flash character.
    // we handle that differently by decoding the char & 0xC0 == 0x40. 
    uint8_t r = 0;
    for (int i = 0; i < 7; i++) {
        bool bit = b & (1 << (6-i)); 
        r |= (bit << i);
    }
    return r;
}

void CharRom::print_matrix(uint16_t tchar) {
    for (int line = 0; line < 8; line++) {
        uint8_t cdata = get_char_scanline(tchar, line);
        // Process each bit
        for (int bit = 0; bit < 7; bit++) {
            printf("%c", (cdata & 1) ? '*' : '.');
            cdata >>= 1;
        }
    printf("\n");
}
}
