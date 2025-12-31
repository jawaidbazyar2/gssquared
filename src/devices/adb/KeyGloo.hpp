#pragma once

#include <SDL3/SDL.h>

#include "ADB_Host.hpp"
#include "ADB_Keyboard.hpp"
#include "ADB_Mouse.hpp"

struct key_code_t {
    uint8_t keycode;
    uint8_t modifiers;
};

#define BUFSIZE 0x10

struct uc_vars_t {
    uint8_t permlang; // char set in high nibble
    uint8_t dlyrpt; // delay-to-repeat & repeat-rate values
    uint8_t dncntr; // delay to repeat and auto-repeat counters
    uint8_t modeflgs; // hold different mode flags set if:
    uint8_t initsp; // initial mouse stepping value
    uint8_t msedlyrp; // mouse start delay and start speed
    uint8_t msetrack; // mouse scaling (tracking)
    uint8_t msemaxsp; // mouse keys max speed
    uint8_t mseaccrt; // mouse keys accelration rate
    uint8_t keyqpt; // various flag bits
    uint8_t respstat; // response / status return from cmd->set
    uint8_t fdbxtra; // various bits
    uint8_t fdbxkeys; // modifier bits (set if pressed)
    uint8_t currmod; // keep track for internal and fdb mod state
    uint8_t prevmod; // previous modifier keys status
    uint8_t ctlshfcnt; // control, shift counts
    uint8_t applecnt; // open apple, solid apple counts
    uint8_t stickstat; // sticky mod status
    uint8_t actmod; // active mode flags set if:
    uint8_t spclbyte; // count for sync during warm start; sys rd/wr; bunch of misc stuff.
    uint8_t lstkey; // last key
    uint8_t lstmod; // last modifier
    uint8_t inpt; // buffer input index (says "pointer"), so this might be absolute memory address.
    uint8_t outpt; // buffer output index
    uint8_t bufmod[BUFSIZE];
    uint8_t bufque[BUFSIZE];
    uint8_t fdbmtrx; // table holding list of current keys down.
    uint8_t fdbdata[8]; // in reverse order, is a stack.
    uint8_t fdbbuf2;
    uint8_t fdbbuf1;
    uint8_t fdbadr; // address of auto poll fdb devices
    uint8_t lang;
    uint8_t matrixcd; // indirect jump vector
    uint8_t code;
    uint8_t xkeys;

    uint32_t kc_read_index = 0;
    uint32_t kc_write_index = 0;
    uint32_t elements_in_buffer = 0;
};

class KeyGloo
{
    private:
        ADB_Host * adb_host = nullptr;

        uint8_t rom[3072];       

        union {
            uint8_t ram[96];
            uc_vars_t vars;
        };

        uint8_t key_mods[16];
        uint8_t key_codes[16];
        uint32_t kc_read_index = 0;
        uint32_t kc_write_index = 0;
        uint32_t elements_in_buffer = 0;

        /*
            byte 1: 7-4: FDB Mouse Address
                    3-0: FDB Keyboard Address
            byte 2: 7-4: char.set
                    3-0: set keyboard layout language
            byte 3: 7-4: repeat delay
                        0: 1/4 sec; 1: 1/2 sec; 2: 3/4 sec; 3: 1 sec; 4: no repeat.
                    3-0: repeat rate
                        0: 40/sec; 1: 30/sec; 2: 24/sec; 3: 20/sec; 4: 15/sec; 5: 11/sec; 6: 8/sec; 7: 4/sec
        */
        uint8_t configuration_bytes[3] = {0};

        uint8_t modes_byte = 0;

        uint8_t cmd[8] = {0};
        uint32_t cmd_index = 0;
        uint32_t cmd_bytes = 1;

        uint8_t response[16] = {0};
        uint32_t response_bytes = 0;
        uint32_t response_index = 0;

        uint8_t error_byte = 0;

        key_code_t key_latch;

        const uint8_t adb_version = 0x06;

    public:
        KeyGloo() {
            adb_host = new ADB_Host();
            adb_host->add_device(0x02, new ADB_Keyboard());
            adb_host->add_device(0x03, new ADB_Mouse());

            kc_read_index = 0;
            kc_write_index = 0;
            key_latch = {0,0};
        };
        ~KeyGloo();
    
        void reset() {
            kc_read_index = 0;
            kc_write_index = 0;
            key_latch = {0,0};
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;
        }

        void abort() {
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;
        }

        void flush() {
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;
            kc_read_index = 0;
            kc_write_index = 0;
            key_latch = {0,0};
        }

        void synch_set_defaults() {
            modes_byte = 0;
            
            configuration_bytes[0] = 0x32; // Mouse = 3, KB = 2
            configuration_bytes[1] = 0x00; // US, US
            configuration_bytes[2] = 0x24; // 3/4 sec delay, 15/sec rate
        }
        
        // there are 


        void execute_command() {
            uint8_t value = cmd[0];
            response_bytes = 0; // by default

            switch (value & 0b11'000000) {
                case 0b00'000000:
                    if (value == 0x01) { // ABORT COMMAND
                        abort();
                    } else if (value == 0x02) { // RESET uC
                        reset();
                    } else if (value == 0x03) { // FLUSH COMMAND
                    } else if (value == 0x04) { // set modes
                        modes_byte = value | cmd[1];
                    } else if (value == 0x05) { // clr modes
                        modes_byte = value & ~cmd[1];
                    } else if (value == 0x06) { // set configuration bytes
                        configuration_bytes[0] = cmd[1];
                        configuration_bytes[1] = cmd[2];
                        configuration_bytes[2] = cmd[3];
                    } else if (value == 0x07) { // SYNCH
                        // set modes byte followed by configuration bytes.
                        // on boot/reset there should be mode that accepts only synch command, and after that we're good.
                        modes_byte = cmd[1];
                        configuration_bytes[0] = cmd[2];
                        configuration_bytes[1] = cmd[3];
                        configuration_bytes[2] = cmd[4];
                        reset();
                    } else if (value == 0x08) { // WRITE uC MEMORY
                        // write 1 byte to uC memory
                        ram[cmd[1]] = cmd[2];
                    } else if (value == 0x09) { // READ uC MEMORY
                        // read 1 byte from uC memory
                        uint16_t addr = cmd[1] | (cmd[2] << 8);
                        if (addr < 96) {
                            response[0] = ram[addr];
                        } else {
                            response[0] = rom[addr - 0x1400];
                        }
                        response_bytes = 1;
                    } else if (value == 0x0A) { // READ MODES BYTE
                        response[0] = modes_byte;
                        response_bytes = 1;
                    } else if (value == 0x0B) { // READ CONFIGURATION BYTES
                        response[0] = configuration_bytes[0];
                        response[1] = configuration_bytes[1];
                        response[2] = configuration_bytes[2];
                        response_bytes = 3;
                    } else if (value == 0x0C) { // READ THEN CLEAR ERROR BYTE
                        response[0] = error_byte;
                        error_byte = 0;
                        response_bytes = 1;
                    } else if (value == 0x0D) { // GET VERSION NUMBER
                        response[0] = adb_version; // Version ?
                        response_bytes = 1;
                    } else if (value == 0x0E) { // READ CHAR SETS AVAILABLE
                        response[0] = 0x08;
                        for (int i = 0; i < 8; i++) {
                            response[i+1] = i;
                        }
                        response_bytes = 9;
                    } else if (value == 0x0F) { // READ LAYOUTS AVAILABLE
                        response[0] = 0x08;
                        for (int i = 0; i < 8; i++) {
                            response[i+1] = i;
                        }
                        response_bytes = 9;
                    } else if (value == 0x10) { // RESET SYSTEM
                        reset();
                    } else if (value == 0x11) { // SEND FDB KEYCODE
                        store_key_to_buffer(cmd[1], 0);
                    } 

                    break;
                case 0b01'000000:
                    if (value == 0x40) { // RESET FDB
                        adb_host->reset(0, 0, 0);
                    } else if (value == 0x48) { // RECEIVE BYTES
                        printf("RECEIVE BYTES - unimplemented\n");
                        // response bytes set after cmd execution
                    } else if ((value & 0b11111000) == 0b01001'000) { // TRANSMIT NUM BYTES
                        printf("TRANSMIT NUM BYTES - unimplemented\n");
                        //adb_host->listen( addr,  cmd,  reg);                       
                    } else if ((value & 0b1111'0000) == 0b0101'0000) { // ENABLE SRQ ON DEVICE
                        printf("ENABLE SRQ ON DEVICE - unimplemented\n");
                    } else if ((value & 0b1111'0000) == 0b0110'0000) { // FLUSH BUFFER ON DEVICE
                        printf("FLUSH BUFFER ON DEVICE - unimplemented\n");
                    } else if ((value & 0b1111'0000) == 0b0111'0000) { // DISABLE SRQ ON DEVICE
                        printf("DISABLE SRQ ON DEVICE - unimplemented\n");
                    }
                    break;
                case 0b10'000000:
                    //if ((value & 0b11'000000) == 0b10'000000) { // TRANSMIT 2 BYTES
                    printf("TRANSMIT 2 BYTES - unimplemented\n");
                    break;
                case 0b11'000000:
                    //if ((value & 0b11'000000) == 0b11'000000) { // POLL FDB DEVICE
                    printf("POLL FDB DEVICE - unimplemented\n");
                    break;
            }
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

        uint8_t read_data_register() {
            if (response_bytes > 0) {
                response_bytes--;
                return cmd[response_index];
            }
            return 0;
        }

        void write_cmd_register(uint8_t value) {
            cmd[0] = value;
            
            if (cmd_bytes > 0) {
                cmd[cmd_index] = value;
                cmd_index++;
                cmd_bytes--;
            } else {
                switch (value & 0b11'000000) {
                    case 0b00'000000:
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
                        } else if (value == 0x0A) { // READ MODES BYTE
                        } else if (value == 0x0B) { // READ CONFIGURATION BYTES
                        } else if (value == 0x0C) { // READ THEN CLEAR ERROR BYTE
                        } else if (value == 0x0D) { // GET VERSION NUMBER
                        } else if (value == 0x0E) { // READ CHAR SETS AVAILABLE
                        } else if (value == 0x0F) { // READ LAYOUTS AVAILABLE
                        } else if (value == 0x10) { // RESET SYSTEM
                        } else if (value == 0x11) { // SEND FDB KEYCODE
                            cmd_bytes = 1;
                        } 
    
                        break;
                    case 0b01'000000:
                        if (value == 0x40) { // RESET FDB
                        } else if (value == 0x48) { // RECEIVE BYTES
                            cmd_bytes = 2;
                            // response bytes set after cmd execution
                        } else if ((value & 0b11111000) == 0b01001'000) { // TRANSMIT NUM BYTES
                            uint8_t num = value & 0b00000111;
                            cmd_bytes = num;
                        } else if ((value & 0b1111'0000) == 0b0101'0000) { // ENABLE SRQ ON DEVICE
                        } else if ((value & 0b1111'0000) == 0b0110'0000) { // FLUSH BUFFER ON DEVICE
                        } else if ((value & 0b1111'0000) == 0b0111'0000) { // DISABLE SRQ ON DEVICE
                        }
                        break;
                    case 0b10'000000:
                        //if ((value & 0b11'000000) == 0b10'000000) { // TRANSMIT 2 BYTES
                        break;
                    case 0b11'000000:
                        //if ((value & 0b11'000000) == 0b11'000000) { // POLL FDB DEVICE
                        break;
                }

            }
            if (cmd_bytes == 0) {
                execute_command();
                cmd_bytes = 1;
            }
        } 

        bool process_event(SDL_Event &event) {
            return adb_host->process_event(event);
        }
};
