#include "scc8530.hpp"
#include "computer.hpp"
#include "Z85C30.hpp"

#include "util/DebugHandlerIDs.hpp"
#include "util/DebugFormatter.hpp"

constexpr uint32_t SCCBREG = 0xC038;
constexpr uint32_t SCCAREG = 0xC039;
constexpr uint32_t SCCBDATA = 0xC03A;
constexpr uint32_t SCCADATA = 0xC03B;

uint8_t scc8530_read_C0xx(void *context, uint32_t address) {
    scc8530_state_t *st = (scc8530_state_t *)context;
    switch (address & 0xFFFF) {
        case SCCBREG:
            return st->scc->readCmd(SCC_CHANNEL_B);
        case SCCAREG:
            return st->scc->readCmd(SCC_CHANNEL_A);
        case SCCBDATA:
            return st->scc->readData(SCC_CHANNEL_B);
        case SCCADATA:
            return st->scc->readData(SCC_CHANNEL_A);
        default:
            assert(false && "Invalid SCC8530 address");
            return 0;
    }
}

void scc8530_write_C0xx(void *context, uint32_t address, uint8_t data) {
    scc8530_state_t *st = (scc8530_state_t *)context;
    switch (address & 0xFFFF) {
        case SCCBREG:
            st->scc->writeCmd(SCC_CHANNEL_B, data);
            break;
        case SCCAREG:
            st->scc->writeCmd(SCC_CHANNEL_A, data);
            break;
        case SCCBDATA:
            st->scc->writeData(SCC_CHANNEL_B, data);
            break;
        case SCCADATA:
            st->scc->writeData(SCC_CHANNEL_A, data);
            break;
        default:
            assert(false && "Invalid SCC8530 address");
            break;
    }
}

void init_scc8530_slot(computer_t *computer, SlotType_t slot) {

    scc8530_state_t *st = new scc8530_state_t();
    st->irq_control = computer->irq_control;

    Z85C30 *scc = new Z85C30(st->irq_control);
    st->scc = scc;

    for (uint32_t i = 0xC038; i <= 0xC03B; i++) {
        computer->mmu->set_C0XX_write_handler(i, { scc8530_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { scc8530_read_C0xx, st });
    }

    const char *data_filename = "scc8530-a.data";
    st->data_file_a = fopen(data_filename, "wb");
    if (st->data_file_a == NULL) {
        fprintf(stderr, "Failed to open data file %s\n", data_filename);
        return;
    }
    scc->set_data_file(SCC_CHANNEL_A, st->data_file_a);
    
    const char *data_filename_b = "scc8530-b.data";
    st->data_file_b = fopen(data_filename_b, "wb");
    if (st->data_file_b == NULL) {
        fprintf(stderr, "Failed to open data file %s\n", data_filename);
        return;
    }
    scc->set_data_file(SCC_CHANNEL_B, st->data_file_b);
    
    computer->register_debug_display_handler(
        "scc8530",
        DH_SCC8530, // unique ID for this, need to have in a header.
        [st]() -> DebugFormatter * {
            DebugFormatter *df = new DebugFormatter();
            st->scc->debug_output(df);
            return df;
        }
    );

}