#include "scc8530.hpp"
#include "computer.hpp"
#include "Z85C30.hpp"

#include "util/Connections.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/DebugFormatter.hpp"
#include "serial_devices/SerialDevice.hpp"

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
    (void)slot;

    scc8530_state_t *st = new scc8530_state_t();
    st->irq_control = computer->irq_control;

    Z85C30 *scc = new Z85C30(st->irq_control, computer->event_timer, computer->clock);
    st->scc = scc;

    for (uint32_t i = 0xC038; i <= 0xC03B; i++) {
        computer->mmu->set_C0XX_write_handler(i, { scc8530_write_C0xx, st });
        computer->mmu->set_C0XX_read_handler(i, { scc8530_read_C0xx, st });
    }

    computer->register_debug_display_handler(
        "scc8530",
        DH_SCC8530,
        [st]() -> DebugFormatter * {
            DebugFormatter *df = new DebugFormatter();
            st->scc->debug_output(df);
            return df;
        }
    );

    // IIgs firmware: Port A = slot 1, Port B = slot 2. Devices attach via Connections.
#if defined(__EMSCRIPTEN__)
    const connection_device_type_t default_b = connection_device_type_t::FILE;
#else
    const connection_device_type_t default_b = connection_device_type_t::MODEM;
#endif

    computer->connections->register_port(
        connection_key_t{1, "a"},
        "Serial Slot 1",
        connection_port_kind_t::SERIAL,
        connection_device_type_t::FILE,
        [st](SerialDevice *dev) {
            st->channel_a_device = dev;
            st->scc->set_device_channel(SCC_CHANNEL_A, dev);
        },
        "A");

    computer->connections->register_port(
        connection_key_t{2, "a"},
        "Serial Slot 2",
        connection_port_kind_t::SERIAL,
        default_b,
        [st](SerialDevice *dev) {
            st->channel_b_device = dev;
            st->scc->set_device_channel(SCC_CHANNEL_B, dev);
        },
        "B");

    computer->register_shutdown_handler([st]() {
        // SerialDevices are owned by Connections; only clear chip pointers here.
        st->channel_a_device = nullptr;
        st->channel_b_device = nullptr;
        delete st->scc;
        delete st;
        return true;
    });

    computer->register_reset_handler([st](bool cold_start) {
        (void)cold_start;
        st->scc->reset();
        return true;
    });
}
