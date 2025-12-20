#include "computer.hpp"
#include "SlotData.hpp"
#include "RTC.hpp"
#include "rtc_pram.hpp"


void rtc_pram_write_C033(void *context, uint32_t address, uint8_t value) {
    rtc_pram_state_t *st = (rtc_pram_state_t *)context;
    printf("write_c033: %02X\n", value);
    st->rtc->write_data_reg(value);
}

uint8_t rtc_pram_read_C033(void *context, uint32_t address) {
    rtc_pram_state_t *st = (rtc_pram_state_t *)context;
    uint8_t val = st->rtc->read_data_reg();
    printf("read_c033: %02X\n", val);
    return val;
}

void rtc_pram_write_C034(void *context, uint32_t address, uint8_t value) {
    printf("write_c034: %02X\n", value);

    rtc_pram_state_t *st = (rtc_pram_state_t *)context;
    if (st->display_wr_handler.write) st->display_wr_handler.write(st->display_wr_handler.context, address, value & 0x0F);
    st->rtc->write_control_reg(value >> 5);
}

uint8_t rtc_pram_read_C034(void *context, uint32_t address) {
    rtc_pram_state_t *st = (rtc_pram_state_t *)context;
    uint8_t dispval;
    if (st->display_rd_handler.read) dispval = st->display_rd_handler.read(st->display_rd_handler.context, address);
    uint8_t rtcval = st->rtc->read_control_reg();
    printf("read_c034: %02X\n", dispval | (rtcval << 5));

    return dispval | (rtcval << 5);
}

void init_slot_rtc_pram(computer_t *computer, SlotType_t slot) {

    rtc_pram_state_t *st = new rtc_pram_state_t();
    st->rtc = new RTC();
    
    computer->mmu->set_C0XX_write_handler(0xC033, { rtc_pram_write_C033, st });
    computer->mmu->set_C0XX_read_handler(0xC033, { rtc_pram_read_C033, st });

    // Get existing C034 handlers - will be from display.
    computer->mmu->get_C0XX_write_handler(0xC034, st->display_wr_handler);
    computer->mmu->get_C0XX_read_handler(0xC034, st->display_rd_handler);

    computer->mmu->set_C0XX_write_handler(0xC034, { rtc_pram_write_C034, st });
    computer->mmu->set_C0XX_read_handler(0xC034, { rtc_pram_read_C034, st });
    
    computer->register_shutdown_handler([st]() {
        delete st->rtc;
        delete st;
        return true;
    });

}