#include <cstdio>
#include <cstdint>

#include "devices/rtc/RTC.hpp"

uint64_t debug_level = DEBUG_RTC;

void transaction (RTC &rtc, bool read_write) {

    rtc.write_control_reg(RTC_START_TRANS | RTC_CLOCK_ENABLE | (read_write << 1)); // start transaction
    
    uint8_t val;
    // wait for transaction to complete
    while (1) {
        val = rtc.read_control_reg();
        if ((val & RTC_START_TRANS) == 0) break;
    }

}

void write_transaction(RTC &rtc) {
    transaction(rtc, false);
}

void read_transaction(RTC &rtc) {
    transaction(rtc, true);
}

uint8_t read_seconds_register_lo(RTC &rtc) {
    uint8_t val;
    printf("[[ Reading seconds_lo\n");
    rtc.write_data_reg(RTC_RD_SECS_LO); // read seconds register lo
    write_transaction(rtc);
    read_transaction(rtc);
    printf("]]> seconds_lo: %02X\n", val);
    val = rtc.read_data_reg();
    return val;
}

uint8_t read_register(RTC &rtc, const char *name, uint8_t command) {
    uint8_t val;
    printf("[[ Reading register %02X (%s)\n", command, name);
    rtc.write_data_reg(command); // read seconds register next-to-lo
    write_transaction(rtc);
    read_transaction(rtc);
    val = rtc.read_data_reg();
    printf("]]> register %02X (%s) = %02X\n", command, name, val);
    return val;
}

uint8_t read_bram(RTC &rtc, uint8_t address) {
    uint8_t ba1,ba2;
    ba1 = (address & 0b111'00000) >> 5;
    ba2 = address & 0b000'11111;
    uint8_t cmd1 = RTC_RD_BRAM | ba1;
    uint8_t cmd2 = ba2 << 2;

    printf("[[ Reading BRAM %02X (cmd1: %02X, cmd2: %02X)\n", address, cmd1, cmd2);

    rtc.write_data_reg(cmd1); // read seconds register next-to-lo
    write_transaction(rtc);

    rtc.write_data_reg(cmd2); // read seconds register next-to-lo
    write_transaction(rtc);

    read_transaction(rtc);
    uint8_t val = rtc.read_data_reg();
    printf("]]> bram %02X = %02X\n", address, val);
    return val;
}

void write_bram(RTC &rtc, uint8_t address, uint8_t value) {
    uint8_t ba1,ba2;
    ba1 = (address & 0b111'00000) >> 5;
    ba2 = address & 0b000'11111;
    uint8_t cmd1 = RTC_WR_BRAM | ba1;
    uint8_t cmd2 = ba2 << 2;

    printf("[[ Writing BRAM %02X (cmd1: %02X, cmd2: %02X)\n", address, cmd1, cmd2);

    rtc.write_data_reg(cmd1); // read seconds register next-to-lo
    write_transaction(rtc);

    rtc.write_data_reg(cmd2); // read seconds register next-to-lo
    write_transaction(rtc);

    rtc.write_data_reg(value); // write value to bram
    write_transaction(rtc);

    printf("]]> bram %02X written\n", address);
}

int main(int argc, char **argv) {
    RTC rtc;
    
    uint8_t val = read_register(rtc, "RD seconds_lo", RTC_RD_SECS_LO);
    uint8_t val2 = read_register(rtc, "RD seconds_next_to_lo", RTC_RD_SECS_NEXT_TO_LO);
    uint8_t val3 = read_register(rtc, "RD seconds_next_to_hi", RTC_RD_SECS_NEXT_TO_HI);
    uint8_t val4 = read_register(rtc, "RD seconds_hi", RTC_RD_SECS_HI);

    printf("%02X%02X%02X%02X\n", val4, val3, val2, val);
    uint32_t seconds = (val4 << 24) | (val3 << 16) | (val2 << 8) | val;
    printf("seconds: %08X\n", seconds);
    // convert seconds to unix epoch and print
    time_t unix_epoch = seconds + UNIX_EPOCH_DELTA;
    printf("%s\n",ctime(&unix_epoch));

    write_bram(rtc, 0x4D, 0xBE);

    uint8_t b4D = read_bram(rtc, 0x4D);
    printf("b4D: %02X\n", b4D);

    return 0;

}
