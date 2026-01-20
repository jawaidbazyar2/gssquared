#include "ensoniq.hpp"
#include "computer.hpp"


uint8_t ensoniq_read_C0xx(void *context, uint32_t address) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    switch (address) {
        case 0xC03C:
            return 0 /* st->soundctl */;
        case 0xC03D:
            return 0x80 /* st->sounddata */;
        case 0xC03E:
            return 0 /* st->soundadrl */;
        case 0xC03F:
            return 0 /* st->soundadrh */;
        default:
            assert(false && "Invalid Ensoniq address");
            return 0;
    }
}

void ensoniq_write_C0xx(void *context, uint32_t address, uint8_t data) {
    ensoniq_state_t *st = (ensoniq_state_t *)context;
    switch (address) {
        case 0xC03C:
            st->soundctl = data;
            break;
        case 0xC03D:
            st->sounddata = data;
            break;
        case 0xC03E:
            st->soundadrl = data;
            break;
        case 0xC03F:
            st->soundadrh = data;
            break;
        default:
            assert(false && "Invalid Ensoniq address");
            break;
    }
}

void init_ensoniq_slot(computer_t *computer, SlotType_t slot) {

    ensoniq_state_t *st = new ensoniq_state_t();

    for (uint32_t i = 0xC03C; i <= 0xC03F; i++) {
        computer->mmu->set_C0XX_write_handler(i, { ensoniq_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { ensoniq_read_C0xx, st });
    }

}