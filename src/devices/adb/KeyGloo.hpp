#pragma once

#include "adb.cpp"

struct key_code_t {
    uint8_t keycode;
    uint8_t modifiers;
};

class KeyGloo
{
    private:
        key_code_t key_buffer[16];
        uint32_t kc_read_index = 0;
        uint32_t kc_write_index = 0;
        uint32_t elements_in_buffer = 0;

        uint8_t cmd[8] = {0};
        uint32_t cmd_index = 0;
        uint32_t cmd_bytes = 1;
        uint32_t response_bytes = 0;

        key_code_t key_latch;

    public:
        KeyGloo() {
            kc_read_index = 0;
            kc_write_index = 0;
            key_latch = {0,0};
        };
        ~KeyGloo();
    
        void execute_command() {
            
        }

        void load_key_from_buffer() {
            if (kc_read_index == kc_write_index) return;
            key_latch = key_buffer[kc_read_index];
            kc_read_index = (kc_read_index + 1) % 16;
        }
        
        void store_key_to_buffer(uint8_t keycode, uint8_t modifiers) {
            if ((kc_write_index + 1) % 16 == kc_read_index) return;
            key_buffer[kc_write_index] = { keycode, modifiers };
            kc_write_index = (kc_write_index + 1) % 16;
            elements_in_buffer = (kc_write_index - kc_read_index + 16) % 16;
            // if the latch is cleared, load it.
            if (key_latch.keycode & 0x80) return;
            load_key_from_buffer();
        }

        uint8_t read_key_latch() {
            return key_latch.keycode;
        }
        uint8_t read_mod_latch() {
            return key_latch.modifiers;
        }
        uint8_t read_key_strobe() {
            key_latch.keycode &= 0x7F; // clear the strobe bit.
        }
        void write_key_strobe() {
            key_latch.keycode &= 0x7F; // clear the strobe bit.
        }
        void write_cmd_register(uint8_t value) {
            cmd[0] = value;
            
            if (cmd_bytes > 0) {
                cmd[cmd_index] = value;
                cmd_index++;
                cmd_bytes--;
            } else {
                if (value == 0x01) { // ABORT COMMAND

                } else if (value == 0x02) { // RESET uC
                } else if (value == 0x03) { // FLUSH COMMAND
                } else if (value == 0x04) { // set modes
                    cmd_bytes = 1;
                } else if (value == 0x05) { // clr modes
                    cmd_bytes = 1;
                } else if (value == 0x06) { // set configuration bytes
                    cmd_bytes = 3;
                } else if (value == 0x07) { // SYNCH
                    // 
                } else if (value == 0x08) { // WRITE uC MEMORY
                    cmd_bytes = 1;
                } else if (value == 0x09) { // READ uC MEMORY
                    cmd_bytes = 2;
                    response_bytes = 1;
                } else if (value == 0x0A) { // READ MODES BYTE
                    response_bytes = 1;
                } else if (value == 0x0B) { // READ CONFIGURATION BYTES
                    response_bytes = 3;
                } else if (value == 0x0C) { // READ THEN CLEAR ERROR BYTE
                } else if (value == 0x0D) { // GET VERSION NUMBER
                } else if (value == 0x0E) { // READ CHAR SETS AVAILABLE
                } else if (value == 0x0F) { // READ LAYOUTS AVAILABLE
                } else if (value == 0x10) { // RESET SYSTEM
                } else if (value == 0x11) { // SEND FDB KEYCODE
                    cmd_bytes = 1;
                } else if (value == 0x40) { // RESET FDB
                } else if (value == 0x48) { // RECEIVE BYTES
                    cmd_bytes = 2;
                    // response bytes set after cmd execution
                } else if ((value & 0b11111000) == 0b01001'000) { // TRANSMIT NUM BYTES
                    uint8_t num = value & 0b00000111;
                    cmd_bytes = num;
                } else if ((value & 0b1111'0000) == 0b0101'0000) { // ENABLE SRQ ON DEVICE
                } else if ((value & 0b1111'0000) == 0b0110'0000) { // FLUSH BUFFER ON DEVICE
                } else if ((value & 0b1111'0000) == 0b0111'0000) { // DISABLE SRQ ON DEVICE
                } else if ((value & 0b11'000000) == 0b10'000000) { // TRANSMIT 2 BYTES
                } else if ((value & 0b11'000000) == 0b11'000000) { // POLL FDB DEVICE
                }
            }
            if (cmd_bytes == 0) {
                execute_command();
                cmd_bytes = 1;
            }
        } 
};
