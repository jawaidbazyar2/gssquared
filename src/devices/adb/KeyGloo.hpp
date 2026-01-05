#pragma once

#include <SDL3/SDL.h>

#include "ADB_Host.hpp"
#include "ADB_Keyboard.hpp"
#include "ADB_Mouse.hpp"
#include "ADB_ASCII.hpp"

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
    adb_mod_key_t currmod; // keep track for internal and fdb mod state
    adb_mod_key_t prevmod; // previous modifier keys status
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

            reset();
            /* vars.inpt = 0;
            vars.outpt = 0;
            key_latch = {0,0}; */
        };
        ~KeyGloo();
    
        void reset() {
            vars.inpt = 0;
            vars.outpt = 0;
            key_latch = {0,0};
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;

            vars.currmod.value = 0;
            vars.prevmod.value = 0;
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
            vars.inpt = 0;
            vars.outpt = 0;
            key_latch = {0,0};
        }

        void synch_set_defaults() {
            modes_byte = 0;
            
            configuration_bytes[0] = 0x32; // Mouse = 3, KB = 2
            configuration_bytes[1] = 0x00; // US, US
            configuration_bytes[2] = 0x24; // 3/4 sec delay, 15/sec rate
        }
        
        // unlike the //e and //+ conversion where we mostly rely on SDL, here we have to fully
        // map our scancode to ascii, taking account of modifiers.
        uint8_t map_us(uint8_t code, adb_mod_key_t mods) {
            if (code >= 0x40) return code;
            uint8_t ascii = adb_ascii_us[code];
            if (mods.ctrl) {
                if (ascii >= 'a' && ascii <= 'z') ascii = ascii - 'a' + 1; // convert to ASCII control code
                if ((mods.shift) && (ascii == '2')) ascii = 0x00; // control-@ is special case
                if ((mods.shift) && (ascii == '6')) ascii = 0x1E; // control-shift-6 is special case
                return ascii;
            }
            if (mods.caps) { // caps lock only affects letters
                if (ascii >= 'a' && ascii <= 'z') ascii = ascii - 'a' + 'A'; // convert to uppercase
                // fall through to shift, which won't match lowercase letters any more, but will match punctuation.
            }
            if (mods.shift) { // shift is used for uppercase and punctuation
                if (ascii >= 'a' && ascii <= 'z') ascii = ascii - 'a' + 'A'; // convert to uppercase
                else {
                    switch (ascii) {
                        case '1': ascii = '!'; break;
                        case '2': ascii = '@'; break;
                        case '3': ascii = '#'; break;
                        case '4': ascii = '$'; break;
                        case '5': ascii = '%'; break;
                        case '6': ascii = '^'; break;
                        case '7': ascii = '&'; break;
                        case '8': ascii = '*'; break;
                        case '9': ascii = '('; break;
                        case '0': ascii = ')'; break;
                        case '-': ascii = '_'; break;
                        case '=': ascii = '+'; break;
                        case '[': ascii = '{'; break;
                        case ']': ascii = '}'; break;
                        case ';': ascii = ':'; break;
                        case '\'': ascii = '"'; break;
                        case ',': ascii = '<'; break;
                        case '.': ascii = '>'; break;
                        case '/': ascii = '?'; break;
                        case '`': ascii = '~'; break;
                    }
                }
            } 
            return ascii;
        }

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
                        // TODO: the docs imply this will take arguments for both the modes_byte and the configuration_bytes.
                        // but I'm not sure about that.
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
            if (vars.inpt == vars.outpt) return;
            // only if there is no key in the latch currently.
            if (key_latch.keycode & 0x80) return;

            key_latch.keycode = key_codes[vars.inpt] | 0x80; // set the 'key present' bit.
            key_latch.keymods.value = key_mods[vars.inpt];
            vars.inpt = (vars.inpt + 1) % 16;
            //elements_in_buffer = (vars.outpt - vars.inpt + 16) % 16;
        }
        
        void store_key_to_buffer(uint8_t keycode, uint8_t keymods) {
            if ((vars.outpt + 1) % 16 == vars.inpt) return;
            key_codes[vars.outpt] = keycode;
            key_mods[vars.outpt] = keymods;
            vars.outpt = (vars.outpt + 1) % 16;
            //elements_in_buffer = (vars.outpt - vars.inpt + 16) % 16;
            // if the latch is cleared, load it.
            if (key_latch.keycode & 0x80) return;
            load_key_from_buffer();
        }

        void print() {
            printf("KG> KeyGloo: currmod: %02X, prevmod: %02X\n", vars.currmod.value, vars.prevmod.value);
            printf("KG> KeyGloo: key_latch: %02X, key_mods: %02X\n", key_latch.keycode, key_latch.keymods.value);
            printf("KG> KeyGloo: key_codes: ");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", key_codes[i]);
            }
            printf("\n");
            printf("KG> KeyGloo: key_mods: ");
            for (int i = 0; i < 16; i++) {
                printf("%02X ", key_mods[i]);
            }
        }

        uint8_t read_key_latch() {  // c000
            return key_latch.keycode;
        }

        uint8_t read_mod_latch() {  // c025
            return key_latch.keymods.value;
        }

        uint8_t read_key_strobe() { // c010
            key_latch.keycode &= 0x7F; // clear the strobe bit.
            load_key_from_buffer();
            return 0xEE; // TODO: return floating bus value.
        }

        void write_key_strobe() { // c010
            key_latch.keycode &= 0x7F; // clear the strobe bit.
            load_key_from_buffer();
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
            // TODO: after processing event, check keyboard register for new key event and read it here.
            bool status = adb_host->process_event(event);
            if (status) {
                ADB_Register reg;
                adb_host->talk(0x02, 0b11, 0, reg);
                // for each key event in returned reg, process and update modifiers.
                // and keycode.
                for (int i = 0; i < reg.size; i++) {
                    if (reg.data[i] == 0xFF) break;
                    uint8_t keycode = reg.data[i] & 0x7F;
                    uint8_t keyupdown = reg.data[i] & 0x80 ? false : true; // true = key down, false = key up
                    switch (keycode) {
                        case ADB_CONTROL: vars.prevmod = vars.currmod; vars.currmod.ctrl = keyupdown; break;
                        case ADB_LEFT_SHIFT: vars.prevmod = vars.currmod; vars.currmod.shift = keyupdown; break;
                        case ADB_COMMAND: vars.prevmod = vars.currmod; vars.currmod.open = keyupdown; break;
                        case ADB_OPTION: vars.prevmod = vars.currmod; vars.currmod.closed = keyupdown; break;
                        case ADB_CAPS_LOCK: 
                            // caps lock is a special case, it toggles the caps lock state, but only on key up.
                            // TODO: should I do it on the key down instead?
                            if (!keyupdown) {
                                vars.prevmod = vars.currmod; 
                                vars.currmod.caps = ! vars.currmod.caps;
                            }
                            break;
                        default:           
                            // TODO: Map the keycode here through a mapper based on the language setting.
                            // TODO: do we get a key in C000 on key down, key up, or ... ?
                                      
                            if (keyupdown) store_key_to_buffer(map_us(keycode, vars.currmod), vars.currmod.value); // TODO: update modifiers.
                            break;
                    }
                }
            }
            print();
            return status;
        }
};
