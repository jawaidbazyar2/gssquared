#include "scc8530.hpp"
#include "computer.hpp"


uint8_t scc8530_read_C0xx(void *context, uint32_t address) {
    scc8530_state_t *st = (scc8530_state_t *)context;
    switch (address) {
        case 0xC038:
            return 0 /* st->cmd_b */;
        case 0xC039:
            return 0 /* st->cmd_a */;
        case 0xC03A:
            return 0 /* st->data_b */;
        case 0xC03B:
            return 0 /* st->data_a */;
        default:
            assert(false && "Invalid SCC8530 address");
            return 0;
    }
}

void scc8530_write_C0xx(void *context, uint32_t address, uint8_t data) {
    scc8530_state_t *st = (scc8530_state_t *)context;
    switch (address) {
        case 0xC038:
            st->cmd_b = data;
            break;
        case 0xC039:
            st->cmd_a = data;
            break;
        case 0xC03A:
            st->data_b = data;
            break;
        case 0xC03B:
            st->data_a = data;
            break;
        default:
            assert(false && "Invalid SCC8530 address");
            break;
    }
}

void init_scc8530_slot(computer_t *computer, SlotType_t slot) {

    scc8530_state_t *st = new scc8530_state_t();

    for (uint32_t i = 0xC038; i <= 0xC03B; i++) {
        computer->mmu->set_C0XX_write_handler(i, { scc8530_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { scc8530_read_C0xx, st });
    }

}