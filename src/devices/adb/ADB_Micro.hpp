#pragma once

#include <SDL3/SDL.h>
#include <cstdio>

#include "util/ResourceFile.hpp"

#include "ADB_Host.hpp"
#include "ADB_Keyboard.hpp"
#include "ADB_Mouse.hpp"
#include "ADB_ASCII.hpp"
#include "SDL3/SDL_events.h"
#include "util/DebugFormatter.hpp"

#define BUFSIZE 0x10

constexpr bool MOUSE_X = false;
constexpr bool MOUSE_Y = true;

/* enum mouse_next_t {
    MOUSE_X,
    MOUSE_Y,
};
 */
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
    uint8_t fdbadr; // address of auto poll fdb devices (hi nib = 3 (mouse) low nib = 2 (kb))
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

        //mouse_next_t mouse_next_read = MOUSE_Y;
        uint8_t mouse_data[2] = {0};

        bool interrupt_asserted = false;
        bool data_interrupt_asserted = false;
        bool send_data_register = false;

        /* bool mouse_interrupt_asserted = false;
        bool kb_interrupt_asserted = false; */
        
        union {
            struct {
                bool command_register_full : 1;
                bool mouse_x_available : 1;
                bool kb_interrupt_enabled : 1; // GS FW Ref says "never use, won't work"
                bool kb_register_full : 1;     // GS FW Ref says "never use, won't work"
                bool data_interrupt_enabled : 1;
                bool data_register_full : 1;
                bool mouse_interrupt_enabled : 1;               
                bool mouse_data_full : 1;
            };
            uint8_t status;
        };

        union {
            struct {
                bool num_response_bytes : 3;
                bool service_request_valid : 1;
                bool buffer_flush_sequence : 1;
                bool desktop_manager_key_sequence : 1;
                bool abort_valid : 1;
                bool response_byte : 1;
            };
            uint8_t datareg;
        };

        int keysdown = 0;

    public:
        KeyGloo() {
            const char *rom_filename = (true /* is rom 01 */) ? "roms/cards/keyglu/rom01-adb-341s0345.bin" : "roms/cards/keyglu/rom03-adb-341s0632-2.bin";

            ResourceFile *rom = new ResourceFile(rom_filename, READ_ONLY);
            if (rom == nullptr) {
                fprintf(stderr, "Failed to load keyglu/%s\n", rom_filename);
                return;
            }
            rom->load();
            uint8_t *rom_data = (uint8_t *)(rom->get_data());
            memcpy(this->rom, rom_data, 3072);
            delete rom;

            // "power on" the RAM here should be all 0's.
            for (int i = 0; i < sizeof(ram); i++) {
                ram[i] = 0;
            }
            adb_host = new ADB_Host();
            adb_host->add_device(0x02, new ADB_Keyboard());
            adb_host->add_device(0x03, new ADB_Mouse());
            //status = 0;

            reset();

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

            vars.currmod.value &= 0b0100; // clear everything except caps lock
            vars.prevmod.value = 0;
            mouse_x_available = MOUSE_X;
            keysdown = 0;
            status = 0;

            datareg = 0;
            data_register_full = false;
        }

        void abort() {
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;
            data_register_full = false;

        }

        void flush() {
            cmd_index = 0;
            cmd_bytes = 1;
            response_index = 0;
            response_bytes = 0;
            vars.inpt = 0;
            vars.outpt = 0;
            key_latch = {0,0};

            keysdown = 0;
            data_register_full = false;

        }

        void flush_buffer() {
            vars.inpt = 0;
            vars.outpt = 0;
            key_latch = {0,0};
            keysdown = 0;
        }

        void set_vals_from_configuration() {
            // TODO: grab values out of the configuration bytes.
            // LANG: 
            vars.lang = configuration_bytes[1] & 0b1111;
            // DLYRPT: 
            vars.dlyrpt = configuration_bytes[2] & 0b1111;
            vars.fdbadr = configuration_bytes[0]; // set kb / mouse addresses
        }

        void synch_set_defaults() {
            modes_byte = 0;
            
            configuration_bytes[0] = 0x32; // Mouse = 3, KB = 2
            configuration_bytes[1] = 0x00; // US, US
            configuration_bytes[2] = 0x24; // 3/4 sec delay, 15/sec rate
            set_vals_from_configuration();
        }
        
        // unlike the //e and //+ conversion where we mostly rely on SDL, here we have to fully
        // map our scancode to ascii, taking account of modifiers.
        uint8_t map_us(uint8_t code, adb_mod_key_t mods) {
            //if (code >= 0x40) return code;

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
                        case '\\': ascii = '|'; break;
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
            response_index = 0;
            // TODO: on a new command, reset the response index. we may have the wrong # of response bytes on one of our commands. or GS might not be reading them all.

            /* printf("ADB_Micro> Executing command: ");
            for (int i = 0; i < cmd_index; i++) {
                printf("%02X ", cmd[i]);
            }
            printf("\n"); */

            switch (value & 0b11'000000) {
                case 0b00'000000:
                    if (value == 0x01) { // ABORT COMMAND
                        abort();
                        update_interrupt_status();
                    } else if (value == 0x02) { // RESET uC
                        reset();
                        update_interrupt_status();
                    } else if (value == 0x03) { // FLUSH COMMAND
                        //printf("FLUSH COMMAND - unimplemented\n");
                        flush();
                        update_interrupt_status();
                    } else if (value == 0x04) { // set modes
                        modes_byte = value | cmd[1];
                    } else if (value == 0x05) { // clr modes
                        modes_byte = value & ~cmd[1];
                    } else if (value == 0x06) { // set configuration bytes
                        configuration_bytes[0] = cmd[1];
                        configuration_bytes[1] = cmd[2];
                        configuration_bytes[2] = cmd[3];
                        set_vals_from_configuration();
                    } else if (value == 0x07) { // SYNCH
                        // this takes arguments for both the modes_byte and the configuration_bytes.
                        // set modes byte followed by configuration bytes.
                        // on boot/reset there should be mode that accepts only synch command, and after that we're good.
                        reset();
                        modes_byte = cmd[1];
                        configuration_bytes[0] = cmd[2];
                        configuration_bytes[1] = cmd[3];
                        configuration_bytes[2] = cmd[4];
                        set_vals_from_configuration();
                        //response_bytes = 1;
                        //response_bytes_reported = 1;
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
                        //response_bytes_reported = 2;
                    } else if (value == 0x0A) { // READ MODES BYTE
                        response[0] = modes_byte;
                        response_bytes = 1;
                        //response_bytes_reported = 2;
                    } else if (value == 0x0B) { // READ CONFIGURATION BYTES
                        response[0] = configuration_bytes[0];
                        response[1] = configuration_bytes[1];
                        response[2] = configuration_bytes[2];
                        response_bytes = 3;
                        //response_bytes_reported = 4;
                    } else if (value == 0x0C) { // READ THEN CLEAR ERROR BYTE
                        response[0] = error_byte;
                        error_byte = 0;
                        response_bytes = 1;
                        //response_bytes_reported = 2;
                    } else if (value == 0x0D) { // GET VERSION NUMBER
                        response[0] = adb_version; // Version ?
                        response_bytes = 1;
                        //response_bytes_reported = 2;
                    } else if (value == 0x0E) { // READ CHAR SETS AVAILABLE (8 char sets, 0-7) should match mega ii
                        response[0] = 8; // TODO: unsure about bytes returned value here, need to read the docs again.
                        for (int i = 0; i < 8; i++) {
                            response[i+1] = i;
                        }
                        response_bytes = 9;
                        //response_bytes_reported = 2;
                    } else if (value == 0x0F) { // READ LAYOUTS AVAILABLE (10 layouts, 0-9)
                        response[0] = 10;
                        for (int i = 0; i < 10; i++) {
                            response[i+1] = i;
                        }
                        response_bytes = 11;
                        //response_bytes_reported = 2;
                    } else if (value == 0x10) { // RESET SYSTEM
                        reset();
                        //response_bytes = 1;
                    } else if (value == 0x11) { // SEND FDB KEYCODE
                        uint8_t keycode = cmd[1];
                        uint8_t kpflag = 0;
                        // TODO: this also needs to update currmod.keypad if a keypad key is being held down?
                        if (keycode >= 0x43 && keycode <= 0x5C) kpflag = 0x10;
                        store_key_to_buffer(map_us(keycode, (adb_mod_key_t){0}), kpflag); // TODO: update modifiers.

                        //store_key_to_buffer(cmd[1], 0);
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
                        /*
                         * command, w/address, is in 2nd byte.
                         * system starts by sending this command followed by between 2 to 8 data bytes (num+1).
                         * which are to be transmitted over FDB. the command sent will be transmitted
                         * directly as the FDB Command byte, which is the first byte received after the transmit num bytes command.
                         */

                        //adb_host->listen( addr,  cmd,  reg);                       
                    } else if ((value & 0b1111'0000) == 0b0101'0000) { // ENABLE SRQ ON DEVICE
                        printf("ENABLE SRQ ON DEVICE - unimplemented\n");
                    } else if ((value & 0b1111'0000) == 0b0110'0000) { // FLUSH BUFFER ON DEVICE
                        printf("FLUSH BUFFER ON DEVICE - unimplemented\n");
                    } else if ((value & 0b1111'0000) == 0b0111'0000) { // DISABLE SRQ ON DEVICE
                        printf("DISABLE SRQ ON DEVICE - unimplemented\n");
                    }
                    break;
                case 0b10'000000: {
                        //if ((value & 0b11'000000) == 0b10'000000) { // TRANSMIT 2 BYTES
                        /* Assumes a two-byte transfer of data using the FDB listen command */
                        printf("TRANSMIT 2 BYTES: %02X: %02X %02X\n", cmd[0], cmd[1], cmd[2]);
                        uint8_t reg = (value & 0b0011'0000) >> 4;
                        uint8_t addr = value & 0x0F;
                        ADB_Register xmit_reg = {0};
                        xmit_reg.size = 2;
                        xmit_reg.data[0] = cmd[1];
                        xmit_reg.data[1] = cmd[2];
                        adb_host->listen(addr, 0b10, reg, xmit_reg);
                        printf("adb->listen addr: %02X, cmd: %02X, reg: %02X, msg: %02X %02X\n", addr, 0b10, reg, xmit_reg.data[0], xmit_reg.data[1]);
                    }
                    break;
                case 0b11'000000:  {     // POLL FDB DEVICE
                        // same as TALK; 
                        // 11xyabcd
                        // xy = register;
                        // abcd = address
                        uint8_t reg = (value & 0b0011'0000) >> 4; // oopsie this was wrong
                        uint8_t addr = value & 0x0F;
                        ADB_Register poll_reg;                        
                        adb_host->talk(addr, 0b11, reg, poll_reg);
                        response_bytes = 2;
                        response_byte = 1;
                        response[0] = poll_reg.data[0];
                        response[1] = poll_reg.data[1];
                        printf("adb->talk addr: %02X, cmd: %02X, reg: %02X, msg: %02X %02X\n", addr, 0b11, reg, poll_reg.data[0], poll_reg.data[1]);
                    }
                    break;
            }

            if (response_bytes > 0) data_register_full = true;
            update_interrupt_status();
            if (data_interrupt_asserted) send_data_register = true;
            /* printf("ADB_Micro> Response (%d bytes/ %d): ", response_bytes, response_index);
            for (int i = 0; i < response_bytes; i++) { 
                printf("%02X ", response[i]);
            }
            printf("\n"); */
        }

        void load_key_from_buffer() {
            if (vars.inpt == vars.outpt) return;
            // only if there is no key in the latch currently.
            if (key_latch.keycode & 0x80) return;

            key_latch.keycode = key_codes[vars.inpt] | 0x80; // set the 'key present' bit.
            key_latch.keymods.value = key_mods[vars.inpt];
            vars.inpt = (vars.inpt + 1) % 16;
            //elements_in_buffer = (vars.outpt - vars.inpt + 16) % 16;
            kb_register_full = true;
            update_interrupt_status();
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

        void print_keyboard() {
            printf("KG> KeyGloo: currmod: %02X, prevmod: %02X\n", vars.currmod.value, vars.prevmod.value);
            printf("KG> KeyGloo: key_latch: %02X, key_mods: %02X\n", key_latch.keycode, key_latch.keymods.value);
            printf("KG> KeyGloo: key_codes: ");
            
            uint8_t indx = vars.inpt;
            while (indx != vars.outpt) {
                printf("%02X ", key_codes[indx]);
                indx = (indx + 1) % 16;
            }
            printf("\n");
            printf("KG> KeyGloo: key_mods : ");
            indx = vars.inpt;
            while (indx != vars.outpt) {
                printf("%02X ", key_mods[indx]);
                indx = (indx + 1) % 16;
            }
            printf("\n");
        }

        void print_mouse() {
            printf("KG> Mouse: mouse_data: %02X, %02X\n", mouse_data[0], mouse_data[1]);
        }

        uint8_t read_key_latch() {  // c000
            return key_latch.keycode;
        }

        uint8_t read_mod_latch() {  // c025
            if (!(key_latch.keycode & 0x80)) return vars.currmod.value; // if no key in latch, return current "live" modifiers.
            return key_latch.keymods.value;
        }

        uint8_t read_key_strobe() { // c010
            key_latch.keycode &= 0x7F; // clear the strobe bit.
            uint8_t retval = key_latch.keycode;

            // clear interrupt status
            kb_register_full = false;
            update_interrupt_status();

            // potentially load a new key from buffer.
            load_key_from_buffer();
            
            return (keysdown > 0 ? 0x80 : 0x00) | retval; // TODO: detect AKD and set hi bit if needed.
        }

        void write_key_strobe(uint8_t value) { // c010
            key_latch.keycode &= 0x7F; // clear the strobe bit.
            
            // clear interrupt status
            kb_register_full = false;
            update_interrupt_status();

            // potentially load a new key from buffer.
            load_key_from_buffer();
        }

        void update_data_register_full() {
            data_register_full = send_data_register || (response_bytes > 0);
        }

        uint8_t read_data_register() {
            // at interrupt time we need to return this.
            /* response[0] = (response_bytes > 0 ? 0x80 : 0x00) |
            (error_byte > 0 ? 0x40 : 0x00) |
            ( false ? 0x20 : 0x00) | // reset_sequence
            ( false ? 0x10 : 0x00) | // buffer_flush_sequence
            ( false ? 0x08 : 0x00) | // service_request_valid
            ((response_bytes_reported-1) & 0x07); */
            if (send_data_register) {
//            if ((data_register_full) && (data_interrupt_enabled)) {
                num_response_bytes = (response_bytes > 0 ? response_bytes-1 : 0); // TODO: jb 2/26/26
                uint8_t retval = datareg; // value before we modify it..
                send_data_register = false;
                response_byte = 0; // TODO: jb 2/26/26
                update_data_register_full();
                desktop_manager_key_sequence = false; // deassert this also
                service_request_valid = false; // deassert this also // TODO: jb 2/27/26
                update_interrupt_status();
                return retval;
            }

            uint8_t retval = 0;

            data_register_full = false; // clear it
            if (response_bytes > 0) {
                response_bytes--;
                
                retval = response[response_index++];
            } else {
                response_index = 0;
                data_register_full = false;
            }

            update_data_register_full();

            // clear interrupt status
            // TODO: should we clear the first byte read, or the last byte?
            update_interrupt_status();
            
            return retval;
        }

        uint8_t read_status_register() {
            return status;
        }
        void write_status_register(uint8_t value) {
            constexpr uint8_t readonly_flags = 0b10101011;
            uint8_t cur_value = status & readonly_flags; // these flags are readonly.
            status = cur_value | (value & ~readonly_flags);
            update_interrupt_status();
        }
        
        /*
| Bit | Name | Description |
|-|-|-|
| 7 | Response/status | When this bit is 1, the ADB microcontroller has received a response from an ADB device previously addressed. 0: No response.|
| 6 | Abort/CTRLSTB flush | When this bit is 1, and only this bit in the register is 1, the ADB microcontroller has encountered an error and has reset itself. When this bit is 1 and bit 4 is also 1, the ADB microcontroller should clear the key strobe (bit 7 in the Keyboard Data register at $C000). |
| 5 | Reset key sequence | When this bit is 1, the Control, Command, and Reset keys have been pressed simultaneously. This condition is usually used to initiate a cold start up |
| 4 | Buffer flush key sequence | When this bit is 1, the Control, Command, and Delete keys have been pressed simultaneously. This condition wil result in the ADB microcontroller's flushing al internally buffered commands. |
| 3 | Service request valid | When this bit is 1, a valid service request is pending. The ADB microcontroller will then poll the ADB devices and determine which has initiated the request |
| 2 - 0 | Number of data bytes returned | The number of data bytes to be returned from the command is listed here. |
*/

        void write_cmd_register(uint8_t value) {
            //cmd[0] = value;
            
            if (cmd_bytes > 0) {
                cmd[cmd_index] = value;
                cmd_index++;
                cmd_bytes--;
            }
            if (cmd_index == 1) {  // after first byte, look at command code and see if there are more bytes to be sent.
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
                            cmd_bytes = 4; // 1 byte modes, 3 bytes configuration
                        } else if (value == 0x08) { // WRITE uC MEMORY
                            cmd_bytes = 2; // 1 byte address (ram only), 1 byte data
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
                        } else {
                            printf("ADB_Micro> Unimplemented command: %02X\n", value);
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
                        } else {
                            printf("ADB_Micro> Unimplemented command: %02X\n", value);
                        }
                        break;
                    case 0b10'000000:
                        //if ((value & 0b11'000000) == 0b10'000000) { // TRANSMIT 2 BYTES
                        cmd_bytes = 2;
                        break;
                    case 0b11'000000:
                        //if ((value & 0b11'000000) == 0b11'000000) { // POLL FDB DEVICE
                        break;
                }

            }
            if (cmd_bytes == 0) {
                execute_command();
                cmd_bytes = 1; // reset for next command
                cmd_index = 0;
            }
        } 

        /*
        the KeyGloo.cpp file reads this and passes interrupt up to CPU. 
        this should be called and interrupt status updated to CPU whenever a register is read, or,
        an event is submitted for processing.
        */ 
    
        bool interrupt_status() {
            return interrupt_asserted;
        }
        bool data_interrupt_status() {
            return data_interrupt_asserted;
        }

        void update_interrupt_status() {
            interrupt_asserted = 
                (mouse_interrupt_enabled && mouse_data_full) ||
                //(data_interrupt_enabled && data_register_full) ||
                (kb_interrupt_enabled && kb_register_full);
            if (interrupt_asserted) {
                assert(true);
            }
            data_interrupt_asserted = data_interrupt_enabled && data_register_full;
            if (data_interrupt_asserted) {
                assert(true);
            }
        }

        // toggle between returning the X data, and the Y data.
        uint8_t read_mouse_data() {
            if (mouse_x_available == MOUSE_Y) {
                mouse_data_full = false;
                mouse_x_available = MOUSE_X; // means X is now available. Cortland doc contradicts IIgs HW Ref. (x=0, y=1)
                update_interrupt_status();
                return mouse_data[0];
            } else { // mouse_x_available == MOUSE_X}
                mouse_x_available = MOUSE_Y; // true means Y is now avail (switching back to Y)
                update_interrupt_status();
                return mouse_data[1];
            }
        }

        bool process_event(SDL_Event &event) {
            
            // if modes_byte indicates no keyboard auto-poll, disregard repeated key events.
            if ((modes_byte & 0x01) != 0 && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.repeat) return true;
            
            // TODO: need to track which keys are down in this function.
            // TODO: after processing event, check keyboard register for new key event and read it here.
            bool status = adb_host->process_event(event);

            if (status && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
                if (modes_byte & 0x01) { // keyboard auto-poll is disabled...
                    // assert SRQ ..
                    service_request_valid = true;
                    data_register_full = true;
                    send_data_register = true;
                    update_interrupt_status();
                    return status;
                }

                ADB_Register reg;
                bool auto_repeat = false;
                adb_host->talk(0x02, 0b11, 0, reg);

                // Discard auto-repeat events.
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.repeat) auto_repeat = true;

                // for each key event in returned reg, process and update modifiers.
                // and keycode.
                // process from LSB to MSB. (i.e., byte 0 (low) first then byte 1 (high))
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
                            // Send key to C000 on key up
                            // TODO: we may need to ignore auto-repeat events and autorepeat keys ourselves,
                            // for instance for the benefit of games that might control that.
                            if (!auto_repeat) { // don't look at auto-repeat events for keysdown counter
                                if (keyupdown) keysdown++;
                                else keysdown--;
                            }

                            // TODO: Map the keycode here through a mapper based on the language setting.
                            if ((keycode == ADB_DELETE) && vars.currmod.ctrl && vars.currmod.open) flush_buffer();
                            else if ((keycode == ADB_ESCAPE) && vars.currmod.ctrl && vars.currmod.open) {
                                if (!keyupdown) { // only assert on key down.
                                    desktop_manager_key_sequence = 1;
                                    send_data_register = true;
                                    data_register_full = 1;
                                    update_interrupt_status();
                                }
                            }
                            else {
                                uint8_t kpflag = 0;
                                // TODO: this also needs to update currmod.keypad if a keypad key is being held down?
                                if (keycode >= 0x43 && keycode <= 0x5C) kpflag = 0x10;
                                if (keyupdown) store_key_to_buffer(map_us(keycode, vars.currmod), vars.currmod.value | kpflag); // TODO: update modifiers.
                            }
                            break;
                    }
                }
                //print_keyboard();

            } else if (status && (event.type == SDL_EVENT_MOUSE_MOTION || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)) {
                ADB_Register reg;
                adb_host->talk(0x03, 0b11, 0, reg);
                /*
                    * Mouse Register 0
                    * Bit 15: Button Pressed
                    * Bit 14: Moved Up
                    * Bit 13-8: Y move value
                    * Bit 7: always 1
                    * Bit 6: Moved right
                    * Bit 5-0: X move value
                */
                /* uint8_t mouse_status = (reg.data[1] & 0x80);
                uint8_t mouse_x = (reg.data[0] & 0x7F);
                uint8_t mouse_y = (reg.data[1] & 0x7F);
                mouse_data[0] = mouse_y | mouse_status;
                mouse_data[1] = mouse_x | mouse_status; */
                mouse_data[0] = reg.data[1];
                mouse_data[1] = reg.data[0];
                mouse_data_full = true;
                mouse_x_available = MOUSE_X;
                //print_mouse();
                update_interrupt_status();
            }

            return status;
        }

        void debug_display(DebugFormatter *df) {
            df->addLine("uC Mode: %02X", modes_byte);
            df->addLine("C027 Status: %02X DataReg: %02X", status, datareg);

            df->addLine("MS/KB Int: %d Data Int: %d", interrupt_asserted, data_interrupt_asserted);

            char rbuf[256] = {0};
            char cbuf[256] = {0};
            char buf[4];

            for (int i = 0; i < cmd_index; i++) {
                snprintf(buf, sizeof(buf), "%02X ", response[i]);
                strncat(rbuf, buf, 250);
            }
            df->addLine("Command Buf [%d/%d]: %s", cmd_index, cmd_bytes, rbuf);

            for (int i = response_index; i < response_index +response_bytes; i++) {
                snprintf(buf, sizeof(buf), "%02X ", response[i]);
                strncat(rbuf, buf, 250);
            }
            df->addLine("Response Buf [%d/%d]: %s", response_index, response_bytes, rbuf);


            df->addLine("Keyboard: currmod: %02X, prevmod: %02X", vars.currmod.value, vars.prevmod.value);
            df->addLine("  keysdown: %d", keysdown);

            // show key codes and mods buffer
            char key_codes_str[64] = "";
            char temp[4];
            uint8_t indx = vars.inpt;
            while (indx != vars.outpt) {
                snprintf(temp, sizeof(temp), "%02X ", key_codes[indx]);
                strncat(key_codes_str, temp, sizeof(key_codes_str) - strlen(key_codes_str) - 1);
                indx = (indx + 1) % 16;
            }
            df->addLine("  keys: %s", key_codes_str);
            
            char key_mods_str[64] = "";
            indx = vars.inpt;
            while (indx != vars.outpt) {
                snprintf(temp, sizeof(temp), "%02X ", key_mods[indx]);
                strncat(key_mods_str, temp, sizeof(key_mods_str) - strlen(key_mods_str) - 1);
                indx = (indx + 1) % 16;
            }
            df->addLine("  mods: %s", key_mods_str);
            df->addLine("MouseData: (%d) X=%02X, Y=%02X", mouse_data_full, mouse_data[0], mouse_data[1]);

            adb_host->debug_display(df);

        }
    };
