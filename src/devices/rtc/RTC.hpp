#pragma once

#include <cstdio>
#include <cstdint>
#include <ctime>
#include <cassert>

#include "util/DebugFormatter.hpp"
#include "debug.hpp"

#define RTC_START_TRANS 0b100
#define RTC_READ_WRITE 0b010
#define RTC_CLOCK_ENABLE 0b001

#define RTC_RD_SECS_LO         0b1000'0001
#define RTC_RD_SECS_NEXT_TO_LO 0b1000'0101
#define RTC_RD_SECS_NEXT_TO_HI 0b1000'1001
#define RTC_RD_SECS_HI         0b1000'1101

#define RTC_WR_SECS_LO         0b0000'0001
#define RTC_WR_SECS_NEXT_TO_LO 0b0000'0101
#define RTC_WR_SECS_NEXT_TO_HI 0b0000'1001
#define RTC_WR_SECS_HI         0b0000'1101

#define RTC_RD_BRAM            0b10111000
#define RTC_WR_BRAM            0b00111000


#define UNIX_EPOCH_DELTA       2082844800

struct RTC_Control_Reg {
    bool clock_enable_assert : 1;
    bool read_write : 1; // 1 = read, 0 = write
    bool start_transaction : 1;
};

enum RTC_State {
    RTC_STATE_AWAIT_COMMAND,
    RTC_STATE_AWAIT_COMMAND2,
    RTC_STATE_AWAIT_DATA,
};

class RTC {
    union {
        RTC_Control_Reg ctl_reg;
        uint8_t ctl_reg_byte;
    };

    uint8_t command_reg[2] = {0};
    uint8_t data_reg;

    uint32_t seconds = 0; // seconds since 1904
    uint8_t test_reg = 0;
    uint8_t bram[256] = {0};

    uint8_t transaction_step_count = 0;
    RTC_State state = RTC_STATE_AWAIT_COMMAND;

    const char *bram_filename = "gs2-bram.bin";
public:
    RTC() {
        // preload with gibberish
        for (int i = 0; i < 256; i++) {
            bram[i] = i;
        }
        load_bram_from_file(bram_filename);
    };
    ~RTC() {
        save_bram_to_file(bram_filename);
    };

    inline uint8_t get_bram_value(uint8_t address) {
        return bram[address];
    }

    void write_data_reg(uint8_t value) {
        if (DEBUG(DEBUG_RTC)) printf("  -> data_reg 0x%02X\n", value);
        data_reg = value;
    };

    uint8_t read_data_reg() {
        if (DEBUG(DEBUG_RTC)) printf("  <- data_reg 0x%02X\n", data_reg);
        return data_reg;
    };

    /* 
    states:
    await_command: 1 = waiting for command; 0 = command latched, ready to execute.   

    state: await_command
        write command to C033.
        execute write_tx

        if single byte command set state: await data
        if double byte command, set state: await command2

    state: await_command2
        write command2 to C033.
        execute write_tx
        set state: await data

    state: await_data
      write:
        write data to C033
        execute write_tx
        if command was a write, set the bram value.
        set state: await_command
      read:
        execute read_tx
        if command was a read, get bram value into data_reg
        set state: await_command
        read C033

    */
    // only pass 3 bits to this function - 2-0 corresponding to 7-5 in $C034
    void write_control_reg(uint8_t cmd) {
        if (DEBUG(DEBUG_RTC)) printf("  -> control_reg 0x%02X\n", cmd);

        if (cmd & 0b101) {
            switch(state) {

                case RTC_STATE_AWAIT_COMMAND:
                    command_reg[0] = data_reg;
                    command_reg[1] = 0;
                    // 7=1, 5=1 indicates start transaction.
                    if (DEBUG(DEBUG_RTC)) {
                        if (cmd & 0b010) { // 6 = 1 indicates read
                            printf("AWAIT COMMAND: transaction(read): command0 = %02X ", data_reg);
                        } else { // 6 = 0 indicates write
                            printf("AWAIT COMMAND: transaction(write): command0 = %02X ", data_reg);
                        }
                    }

                    // detect the two-byte command.
                    // IF the first byte of a command, and it matches the pattern, then this is byte 1 of a two-byte command.
                    if ((data_reg & 0b0'1111'000) == 0b0'0111'000) {  //    Followed by 0defgh00. Access BRAM address abcdefgh)
                        if (DEBUG(DEBUG_RTC)) printf(" (2byte)");
                        // leave await_command = true since we need second byte
                        state = RTC_STATE_AWAIT_COMMAND2;
                    } else { // 
                        state = RTC_STATE_AWAIT_DATA;
                    }
                    break;

                case RTC_STATE_AWAIT_COMMAND2:
                    if (DEBUG(DEBUG_RTC)) {
                        if (cmd & 0b010) { // 6 = 1 indicates read
                            printf("AWAIT COMMAND2: transaction(read): command1 = %02X ", data_reg);
                        } else { // 6 = 0 indicates write
                            printf("AWAIT COMMAND2: transaction(write): command1 = %02X ", data_reg);
                        }
                    }
                    command_reg[1] = data_reg;
                    
                    state = RTC_STATE_AWAIT_DATA;
                    break;

                case RTC_STATE_AWAIT_DATA:
                    // execute the command.
                    if (DEBUG(DEBUG_RTC)) {
                        if (cmd & 0b010) { // 6 = 1 indicates read
                            printf("AWAIT DATA transaction(execute read): command[%02X %02X] = ? ", command_reg[0], command_reg[1]);
                        } else {
                            printf("AWAIT DATA transaction(execute write): %02X -> command[%02X %02X]  ", data_reg, command_reg[0], command_reg[1] );
                        }
                    }
                    execute_command();
                    state = RTC_STATE_AWAIT_COMMAND;
                    break;
            }
            transaction_step_count = 2; // TODO: implement counter to simulate a delay. Measure typical delay on real hw. could vary depending on register and command.
            printf("\n");
        }
        ctl_reg_byte = cmd;
    };

    void update_seconds() {
        // read unix epoch seconds
        uint32_t unix_epoch_seconds = time(nullptr);
        // convert to epoch starting Jan 1 1904.
        uint32_t epoch_seconds = unix_epoch_seconds - UNIX_EPOCH_DELTA;
        // store in seconds register
        seconds = epoch_seconds;
    }

    void execute_command() {
        uint8_t cmd = command_reg[0];

        if ((cmd & 0b0'1111111) == 0b0'0000001) { // 000'0001 = seconds register lo
            if (DEBUG(DEBUG_RTC)) printf("   : seconds lo ");
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read\n");
                update_seconds();
                data_reg =  seconds & 0xFF;
            } ; // ignore writes for now
        } else if ((cmd & 0b0'1111111) == 0b0'0000101) { // 000'0101 = seconds register next-to-lo
            if (DEBUG(DEBUG_RTC)) printf("   : seconds next-to-lo ");
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read\n");
                update_seconds();
                data_reg = (seconds >> 8) & 0xFF;
            }
        } else if ((cmd & 0b0'1111111) == 0b0'0001001) { // 000'1001 = seconds register next-to-hi
            if (DEBUG(DEBUG_RTC)) printf("   : seconds next-to-hi ");
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read\n");
                update_seconds();
                data_reg = (seconds >> 16) & 0xFF;
            }
        } else if ((cmd & 0b0'1111111) == 0b0'0001101) { // 000'1101 = seconds register hi
            if (DEBUG(DEBUG_RTC)) printf("   : seconds hi ");
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read\n");
                update_seconds();
                data_reg = (seconds >> 24) & 0xFF;
            }
        } else if ((cmd & 0b0'111'00'11) == 0b0'010'00'01) { // 4 RAM addresses - z010ab01
            if (DEBUG(DEBUG_RTC)) printf("   : ram cmd0: %02X, cmd1: %02X\n", command_reg[0], command_reg[1]);
            uint8_t bram_address = (cmd & 0b0'000'11'00) >> 2;
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read bram(4)[%02X]\n", bram_address);
                data_reg = bram[bram_address];
            } else {
                if (DEBUG(DEBUG_RTC)) printf("write bram(4)[%02X]: %02X\n", bram_address, data_reg);
                bram[bram_address] = data_reg;
            }
        } else if ((cmd & 0b0'1'0000'11) == 0b0'1'0000'01) { // 16 RAM addresses - z1abcd01
            if (DEBUG(DEBUG_RTC)) printf("   : ram cmd0: %02X, cmd1: %02X\n", command_reg[0], command_reg[1]);
            uint8_t bram_address = (cmd & 0b0'0'1111'00) >> 2;
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read bram(16)[%02X]\n", bram_address);
                data_reg = bram[bram_address];
            } else {
                printf("write bram(16)[%02X]: %02X\n", bram_address, data_reg);
                bram[bram_address] = data_reg;
            }
        } else if ((cmd & 0b0'1111'000) == 0b0'0111'000) { // two byte command - it's a 256-bit bram access
            if (DEBUG(DEBUG_RTC)) printf("   : bram cmd0: %02X, cmd1: %02X\n", command_reg[0], command_reg[1]);
            uint8_t bram_address = (cmd & 0b111) << 5;
            bram_address |= ((command_reg[1] & 0b01111100) >> 2);
            if (cmd & 0b1000'0000) {  // 1 = read, 0 = write
                if (DEBUG(DEBUG_RTC)) printf("read bram[%02X]\n", bram_address);
                data_reg = bram[bram_address];
            } else {
                if (DEBUG(DEBUG_RTC)) printf("write bram[%02X]: %02X\n", bram_address, data_reg);
                bram[bram_address] = data_reg;
            }
        } else {
            if (DEBUG(DEBUG_RTC)) printf("   : unknown command 0x%02X\n", cmd);
        }
        // if we didn't match a command, then we don't modify the data reg, basically.
        // if there is a mismatch between the command (above) and the transaction type (read/write) then we'll pass this back.
    }

    void tick_clock() {
        if (ctl_reg.clock_enable_assert && ctl_reg.start_transaction) {
            transaction_step_count--;
            if (transaction_step_count == 0) {
                ctl_reg.start_transaction = false;
            }
        }
    }

    uint8_t read_control_reg() {
        tick_clock();
        if (DEBUG(DEBUG_RTC)) printf("  <- control_reg 0x%02X\n", ctl_reg_byte);
        return ctl_reg_byte;
    };

    void save_bram_to_file(const char *filename) {
        FILE *file = fopen(filename, "wb");
        if (file) {
            fwrite(bram, 1, 256, file);
            fclose(file);
        } else {
            printf("Error: could not open %s for writing\n", filename);
        }
    }

    void load_bram_from_file(const char *filename) {
        FILE *file = fopen(filename, "rb");
        if (file) {
            fread(bram, 1, 256, file);
            fclose(file);
        } else {
            printf("Error: could not open %s for reading\n", filename);
        }
    }

    const char *state_to_string(RTC_State state) {
        switch (state) {
            case RTC_STATE_AWAIT_COMMAND: return "WAIT_CMD";
            case RTC_STATE_AWAIT_COMMAND2: return "WAIT_CMD2";
            case RTC_STATE_AWAIT_DATA: return "WAIT_DATA";
        }
        return "UNKNOWN";
    }

    DebugFormatter *debug_display() {
        update_seconds();
        DebugFormatter *df = new DebugFormatter();
        df->addLine("state: %s cmd0: %02X cmd1: %02X ", state_to_string(state), command_reg[0], command_reg[1]);
        df->addLine("ctl_reg C034 (7-5): %02X data_reg C033: %02X", ctl_reg_byte, data_reg);
        df->addLine("seconds: %08X", seconds);
        for (int i = 0; i < 256; i+=16) {
            df->addLine("%02X: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", i, 
                bram[i], bram[i+1], bram[i+2], bram[i+3],
                bram[i+4], bram[i+5], bram[i+6], bram[i+7],
                bram[i+8], bram[i+9], bram[i+10], bram[i+11],
                bram[i+12], bram[i+13], bram[i+14], bram[i+15]);
        }
        return df;
    }
};
