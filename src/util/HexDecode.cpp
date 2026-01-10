#include "HexDecode.hpp"

char hex_table[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

void decode_hex_byte(char *buffer, uint8_t byte) {
    buffer[0] = hex_table[(byte >> 4) & 0x0F];
    buffer[1] = hex_table[byte & 0x0F];
}

void decode_hex_word(char *buffer, uint16_t word) {
    buffer[0] = hex_table[(word >> 12) & 0x0F];
    buffer[1] = hex_table[(word >> 8) & 0x0F];
    buffer[2] = hex_table[(word >> 4) & 0x0F];
    buffer[3] = hex_table[word & 0x0F];
}

void decode_hex_addr(char *buffer, uint32_t addr) {
    buffer[0] = hex_table[(addr >> 20) & 0x0F];
    buffer[1] = hex_table[(addr >> 16) & 0x0F];
    buffer[2] = hex_table[(addr >> 12) & 0x0F];
    buffer[3] = hex_table[(addr >> 8) & 0x0F];
    buffer[4] = hex_table[(addr >> 4) & 0x0F];
    buffer[5] = hex_table[addr & 0x0F];
}

void decode_ascii(char *buffer, uint8_t byte) {
    byte &= 0x7F;
    if (byte < 0x20 || byte > 0x7E) {
        *buffer = '.';
    } else {
        *buffer = byte;
    }
}