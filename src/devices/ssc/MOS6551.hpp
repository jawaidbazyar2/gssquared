#pragma once

#include <cstdint>
#include <cstdio>

#include "serial_devices/SerialDevice.hpp"
#include "serial_devices/host/HostSerial.hpp"
#include "util/DebugFormatter.hpp"
#include "util/ClockRail.hpp"
#include "util/InterruptController.hpp"
#include "device_irq_id.hpp"

constexpr bool ACIA6551_DEBUG = false;

/* 6551 on the SSC is clocked by a 1.8432 MHz crystal. */
constexpr uint64_t ACIA_XTAL_HZ = 1'843'200;

/* Emulator master clock (14M). */
constexpr uint64_t ACIA_MASTER_CLOCK = 14'318'180;

constexpr float ACIA_MAX_TIMED_BAUD = 115'200.0f;

/**
 * MOS/Synertek 6551 ACIA — register model from the datasheet and Apple SSC docs.
 * TX is paced on the emu thread via the 14M rail; SerialDevice backends run
 * on a child thread and communicate only through SPSC queues.
 */
class MOS6551 {
    InterruptController *irq_control = nullptr;
    C14mRail *c14m = nullptr;
    device_irq_id irq_id = IRQ_SLOT_2;
    uint64_t timer_base_id = 0x65510000ull;
    SerialDevice *device = nullptr;

    /* Hardware DIP SW2-6 gates the IRQ line to the slot; default enabled. */
    bool dip_irq_enabled = true;

    uint8_t status = 0;
    uint8_t command = 0;
    uint8_t control = 0;
    uint8_t rx_data = 0;
    uint8_t tx_data = 0;       /* byte in the transmit shift register */
    uint8_t tx_holding = 0;    /* byte waiting in TDR when shift is busy */
    bool tx_holding_full = false;

    bool tx_in_progress = false;
    bool rx_in_progress = false;
    bool tx_irq_condition = false;
    bool modem_irq_condition = false;
    uint8_t last_modem_inputs = SerialDevice::MODEM_INPUTS_ASSERTED;

    float baud_rate = 9600.0f;

    /* Status register bits */
    static constexpr uint8_t ST_PE = 0x01;
    static constexpr uint8_t ST_FE = 0x02;
    static constexpr uint8_t ST_OE = 0x04;
    static constexpr uint8_t ST_RDRF = 0x08;
    static constexpr uint8_t ST_TDRE = 0x10;
    static constexpr uint8_t ST_DCD = 0x20; /* 1 = no carrier */
    static constexpr uint8_t ST_DSR = 0x40; /* 1 = not ready */
    static constexpr uint8_t ST_IRQ = 0x80;

    /* Command register helpers */
    inline bool dtr_ready() const { return (command & 0x01) != 0; }
    inline bool rx_irq_disabled() const { return (command & 0x02) != 0; }
    inline uint8_t tx_ctrl() const { return (command >> 2) & 0x03; }
    inline bool tx_irq_enabled() const { return tx_ctrl() == 0b01; }
    inline bool rts_low() const {
        uint8_t t = tx_ctrl();
        return t == 0b01 || t == 0b10 || t == 0b11;
    }
    inline bool receiver_enabled() const { return dtr_ready() && rts_low(); }
    inline bool transmitter_enabled() const { return dtr_ready() && tx_ctrl() != 0b00; }

public:
    MOS6551(InterruptController *irq_control, C14mRail *c14m,
            device_irq_id irq_id, uint64_t timer_base_id = 0x65510000ull)
        : irq_control(irq_control),
          c14m(c14m),
          irq_id(irq_id),
          timer_base_id(timer_base_id) {
        reset();
    }

    void set_device(SerialDevice *dev) {
        device = dev;
        push_line_params();
        apply_modem_inputs();
    }

    bool cts_asserted() const {
        if (device == nullptr) {
            return true;
        }
        return (device->modem_inputs() & SerialDevice::MODEM_CTS) != 0;
    }

    void poll_modem_inputs() {
        apply_modem_inputs();
        update_queues();
    }

    void set_dip_irq_enabled(bool enabled) {
        dip_irq_enabled = enabled;
        update_interrupts();
    }

    void reset() {
        if (c14m) {
            c14m->cancel(timer_base_id + 0); /* TX */
            c14m->cancel(timer_base_id + 1); /* RX */
        }
        status = ST_TDRE; /* TDRE set; DCD/DSR asserted (bits clear) */
        command = 0;
        control = 0;
        rx_data = 0;
        tx_data = 0;
        tx_holding = 0;
        tx_holding_full = false;
        tx_in_progress = false;
        rx_in_progress = false;
        tx_irq_condition = false;
        modem_irq_condition = false;
        last_modem_inputs = SerialDevice::MODEM_INPUTS_ASSERTED;
        baud_rate = 0.0f;
        update_baud();
        apply_modem_inputs();
        update_interrupts();
    }

    uint8_t read_data() {
        uint8_t val = rx_data;
        status &= ~ST_RDRF;
        /* Reading data clears IRQ caused by RDRF */
        update_interrupts();
        /* Start baud-timed reception of the next queued byte immediately. */
        update_queues();
        if (ACIA6551_DEBUG) {
            printf("6551: READ DATA = %02X\n", val);
        }
        return val;
    }

    void write_data(uint8_t data) {
        update_queues();
        if (ACIA6551_DEBUG) {
            printf("6551: WRITE DATA = %02X (tx_en=%d tdre=%d shift=%d)\n", data,
                   transmitter_enabled(), (status & ST_TDRE) != 0, tx_in_progress);
        }
        if (!transmitter_enabled()) {
            return;
        }
        if (!(status & ST_TDRE)) {
            /* Holding register still full — overrun, drop */
            if (ACIA6551_DEBUG) {
                printf("6551: TX overrun, dropping %02X\n", data);
            }
            return;
        }

        if (!tx_in_progress) {
            /* Shift register free — load and start transmission immediately. */
            start_tx_shift(data);
            status |= ST_TDRE;
            tx_irq_condition = true;
        } else {
            /* Shift busy — park byte in holding register until shift completes. */
            tx_holding = data;
            tx_holding_full = true;
            status &= ~ST_TDRE;
            tx_irq_condition = false;
        }
        update_interrupts();
    }

    uint8_t read_status() {
        update_queues();
        uint8_t val = status;
        /* Reading status clears IRQ bit (hardware clears /IRQ); keep condition bits */
        status &= ~ST_IRQ;
        modem_irq_condition = false;
        if (irq_control) {
            irq_control->set_irq(irq_id, false);
        }
        if (ACIA6551_DEBUG) {
            printf("6551: READ STATUS = %02X\n", val);
        }
        return val;
    }

    void write_reset(uint8_t /*data*/) {
        if (ACIA6551_DEBUG) {
            printf("6551: programmed reset\n");
        }
        reset();
    }

    uint8_t read_command() {
        /* Datasheet: write-only. We still echo the latch; SSC firmware RMWs $C0nA. */
        return command;
    }

    void write_command(uint8_t data) {
        if (ACIA6551_DEBUG) {
            printf("6551: WRITE COMMAND = %02X\n", data);
        }
        command = data;
        if (tx_ctrl() == 0b11) {
            /* Break — not messaged to backends in v1 */
        }
        push_line_params();
        apply_modem_inputs();
        update_interrupts();
    }

    uint8_t read_control() {
        /* Datasheet: write-only. Echoing the latch lets ProTERM (and SSC
         * firmware RMW) see a later $16 as 300 baud. */
        uint32_t published = (baud_rate <= 0.0f) ? 9600u
                            : static_cast<uint32_t>(baud_rate + 0.5f);
        printf("6551: READ CONTROL=%02X baud=%u\n", control, published);
        return control;
    }

    void write_control(uint8_t data) {
        control = data;
        update_baud();
        uint32_t published = (baud_rate <= 0.0f) ? 9600u
                            : static_cast<uint32_t>(baud_rate + 0.5f);
        printf("6551: WRITE CONTROL=%02X baud=%u\n", data, published);
        push_line_params();
        apply_modem_inputs();
    }

    void debug_output(DebugFormatter *df) {
        df->addLine("6551 Status:  %02X  Command: %02X  Control: %02X", status, command, control);
        df->addLine("6551 RX: %02X  TX: %02X  baud: %.2f", rx_data, tx_data, baud_rate);
        df->addLine("6551 tx_busy=%d rx_busy=%d dip_irq=%d dcd=%d dsr=%d cts=%d",
                    tx_in_progress, rx_in_progress, dip_irq_enabled,
                    (status & ST_DCD) == 0, (status & ST_DSR) == 0, cts_asserted());
    }

private:
    static constexpr float baud_table[16] = {
        0.0f,     /* 0000 external 16x */
        50.0f,    /* 0001 */
        75.0f,    /* 0010 */
        109.92f,  /* 0011 */
        134.58f,  /* 0100 */
        150.0f,   /* 0101 */
        300.0f,   /* 0110 */
        600.0f,   /* 0111 */
        1200.0f,  /* 1000 */
        1800.0f,  /* 1001 */
        2400.0f,  /* 1010 */
        3600.0f,  /* 1011 */
        4800.0f,  /* 1100 */
        7200.0f,  /* 1101 */
        9600.0f,  /* 1110 */
        19200.0f, /* 1111 */
    };

    void update_baud() {
        uint8_t sel = control & 0x0F;
        if ((control & 0x10) == 0 || sel == 0) {
            /* External clock — treat as untimed / immediate */
            baud_rate = 0.0f;
        } else {
            baud_rate = baud_table[sel];
        }
    }

    void push_line_params() {
        if (device == nullptr) {
            return;
        }
        host_serial_line_t line;
        line.baud = (baud_rate <= 0.0f) ? 9600 : static_cast<uint32_t>(baud_rate + 0.5f);
        switch ((control >> 5) & 0x03) {
            case 0: line.data_bits = 8; break;
            case 1: line.data_bits = 7; break;
            case 2: line.data_bits = 6; break;
            default: line.data_bits = 5; break;
        }
        if ((command & 0x20) == 0) {
            line.parity = HOST_SERIAL_PARITY_NONE;
        } else {
            switch ((command >> 6) & 0x03) {
                case 0: line.parity = HOST_SERIAL_PARITY_ODD; break;
                case 1: line.parity = HOST_SERIAL_PARITY_EVEN; break;
                case 2: line.parity = HOST_SERIAL_PARITY_MARK; break;
                default: line.parity = HOST_SERIAL_PARITY_SPACE; break;
            }
        }
        if (control & 0x80) {
            line.stop_bits = (line.data_bits == 5) ? 15 : 2;
        } else {
            line.stop_bits = 1;
        }
        device->q_host.send(SerialMessage{MESSAGE_LINE, host_serial_pack_line(line)});
    }

    uint8_t bits_per_char() const {
        uint8_t bits = 1; /* start */
        switch ((control >> 5) & 0x03) {
            case 0: bits += 8; break;
            case 1: bits += 7; break;
            case 2: bits += 6; break;
            case 3: bits += 5; break;
        }
        if (command & 0x20) {
            bits += 1; /* parity */
        }
        if (control & 0x80) {
            bits += 2; /* 2 stop (approx; ignore 1.5 special case) */
        } else {
            bits += 1;
        }
        return bits;
    }

    uint64_t get_cycles_per_char() const {
        if (baud_rate <= 0.0f || baud_rate > ACIA_MAX_TIMED_BAUD) {
            return 0;
        }
        float cps = baud_rate / static_cast<float>(bits_per_char());
        return static_cast<uint64_t>(ACIA_MASTER_CLOCK / cps);
    }

    void apply_modem_inputs() {
        if (device == nullptr) {
            return;
        }
        const uint8_t cur = device->modem_inputs();
        const uint8_t prev = last_modem_inputs;
        last_modem_inputs = cur;

        if (cur & SerialDevice::MODEM_CD) {
            status &= ~ST_DCD;
        } else {
            status |= ST_DCD;
        }
        if (cur & SerialDevice::MODEM_DSR) {
            status &= ~ST_DSR;
        } else {
            status |= ST_DSR;
        }

        const uint8_t changed = (prev ^ cur) & (SerialDevice::MODEM_CD | SerialDevice::MODEM_DSR);
        if (changed && dtr_ready()) {
            modem_irq_condition = true;
        }
        update_interrupts();
    }

    void update_queues() {
        apply_modem_inputs();
        if (device == nullptr) {
            return;
        }
        if ((status & ST_RDRF) || rx_in_progress) {
            return;
        }
        /* DTR/RTS off still delivers if carrier is up (CONNECT after ATD). */
        if (!receiver_enabled() && (status & ST_DCD)) {
            return;
        }
        SerialMessage msg = device->q_dev.get();
        if (msg.type == MESSAGE_DATA) {
            schedule_rx_char(static_cast<uint8_t>(msg.data));
        }
    }

    void schedule_rx_char(uint8_t data) {
        if (!receiver_enabled() && (status & ST_DCD)) {
            return;
        }
        if (status & ST_RDRF) {
            status |= ST_OE;
            update_interrupts();
            return;
        }
        rx_data = data;
        rx_in_progress = true;
        /* Immediate: a stalled RX event leaves CONNECT unread and ProTERM
         * sitting on "waiting for connect". TX stays baud-paced. */
        rx_complete();
    }

    void start_tx_shift(uint8_t data) {
        tx_data = data;
        tx_in_progress = true;
        uint64_t cycles = get_cycles_per_char();
        if (cycles > 0 && c14m) {
            c14m->schedule_after(cycles, tx_complete_callback, timer_base_id + 0, this);
        } else {
            tx_complete();
        }
    }

    void tx_complete() {
        tx_in_progress = false;
        if (device != nullptr) {
            device->q_host.send(SerialMessage{MESSAGE_DATA, tx_data});
        }
        if (ACIA6551_DEBUG) {
            printf("6551: TX complete %02X\n", tx_data);
        }
        if (tx_holding_full) {
            tx_holding_full = false;
            start_tx_shift(tx_holding);
            status |= ST_TDRE;
            tx_irq_condition = true;
        } else {
            status |= ST_TDRE;
            tx_irq_condition = true;
        }
        update_interrupts();
    }

    void rx_complete() {
        rx_in_progress = false;
        status |= ST_RDRF;
        if (ACIA6551_DEBUG) {
            printf("6551: RX complete %02X\n", rx_data);
        }
        update_interrupts();
    }

    void update_interrupts() {
        bool irq = false;
        if (dip_irq_enabled && dtr_ready()) {
            if (!rx_irq_disabled() && (status & ST_RDRF)) {
                irq = true;
            }
            if (tx_irq_enabled() && tx_irq_condition && (status & ST_TDRE)) {
                irq = true;
            }
            if (modem_irq_condition) {
                irq = true;
            }
        }
        if (irq) {
            status |= ST_IRQ;
        } else {
            status &= ~ST_IRQ;
        }
        if (irq_control) {
            irq_control->set_irq(irq_id, irq);
        }
    }

    static void tx_complete_callback(uint64_t /*instanceID*/, void *userData) {
        static_cast<MOS6551 *>(userData)->tx_complete();
    }

    static void rx_complete_callback(uint64_t /*instanceID*/, void *userData) {
        static_cast<MOS6551 *>(userData)->rx_complete();
    }
};
