#include "iwm.hpp"
#include "computer.hpp"


uint8_t iwm_read_C0xx(void *context, uint32_t address) {
    static uint8_t random = 0;
    iwm_state_t *st = (iwm_state_t *)context;
    switch (address) {
        case 0xC0E8:
            return 0 /* st->cmd_b */;
        case 0xC0E9:
            return 0 /* st->cmd_a */;
        case 0xC0EA:
            return 0 /* st->data_b */;
        case 0xC0EB:
            return 0 /* st->data_a */;
        default:
            return 0; // random++;
    }
}

void iwm_write_C0xx(void *context, uint32_t address, uint8_t data) {
    iwm_state_t *st = (iwm_state_t *)context;
    switch (address) {
        case 0xC0E8:
            st->status = data;
            break;
        case 0xC0E9:
            st->control = data;
            break;
        case 0xC0EA:
            st->data = data;
            break;
        case 0xC0EB:
            st->data = data;
            break;
        default:
            break;
    }
}

void init_iwm_slot(computer_t *computer, SlotType_t slot) {

    iwm_state_t *st = new iwm_state_t();

    for (uint32_t i = 0xC0E0; i <= 0xC0EF; i++) {
        computer->mmu->set_C0XX_write_handler(i, { iwm_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { iwm_read_C0xx, st });
    }

}