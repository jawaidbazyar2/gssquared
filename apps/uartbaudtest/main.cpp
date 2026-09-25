/*
 * Host-side check that SCC and 6551 receive is paced on the 14 MHz rail.
 * One 8N1 character is MASTER_CLOCK * bits / baud ticks. 2400 8N1 is 10 bits.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>

#include "NClock.hpp"
#include "devices/scc8530/Z85C30.hpp"
#include "devices/ssc/MOS6551.hpp"
#include "serial_devices/SerialDevice.hpp"
#include "util/InterruptController.hpp"

uint64_t debug_level = 0;

static int g_fails = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        g_fails++; \
    } \
} while (0)

class StubSerial : public SerialDevice {
public:
    StubSerial() : SerialDevice("uartbaudtest", "T") {}
    ~StubSerial() override {
        /* Join while this override is still the dynamic type. The base
         * destructor runs too late: the worker may not have entered
         * device_loop yet, and that virtual call would be pure. */
        if (thread) {
            q_host.send(SerialMessage{MESSAGE_SHUTDOWN, 0});
            SDL_WaitThread(thread, nullptr);
            thread = nullptr;
        }
    }
    void device_loop() override {
        while (true) {
            SDL_Delay(5);
            while (!q_host.is_empty()) {
                SerialMessage msg = q_host.get();
                if (msg.type == MESSAGE_SHUTDOWN) {
                    return;
                }
            }
        }
    }
};

static uint64_t cycles_8n1(float baud) {
    float cps = baud / 10.0f;
    return static_cast<uint64_t>(static_cast<float>(MASTER_CLOCK) / cps);
}

static void advance_just_before(NClock &clock) {
    uint64_t due = clock.c14m.next_event().v;
    uint64_t now = clock.get_c14m();
    CHECK(due > now + 1);
    clock.adjust_c14m(due - now - 1);
    clock.process_due();
}

static void advance_one(NClock &clock) {
    clock.adjust_c14m(1);
    clock.process_due();
}

static void scc_wr(Z85C30 &scc, scc_channel_t ch, uint8_t reg, uint8_t val) {
    uint8_t wr0 = static_cast<uint8_t>(reg & 7);
    if (reg >= 8) {
        wr0 = static_cast<uint8_t>(wr0 | 0x08);
    }
    scc.writeCmd(ch, wr0);
    scc.writeCmd(ch, val);
}

static void program_scc_2400(Z85C30 &scc) {
    scc_wr(scc, SCC_CHANNEL_B, WR9, 0x08);          /* MIE */
    scc_wr(scc, SCC_CHANNEL_B, WR1, 0x10);          /* RX int on all characters */
    scc_wr(scc, SCC_CHANNEL_B, WR3, 0xC1);          /* RX enable, 8 data bits */
    scc_wr(scc, SCC_CHANNEL_B, WR5, 0xEA);          /* DTR, RTS, TX enable, 8 bits */
    scc_wr(scc, SCC_CHANNEL_B, WR4, 0x44);          /* x16, 1 stop, no parity */
    scc_wr(scc, SCC_CHANNEL_B, WR12, 46);           /* time constant 46 -> 2400 */
    scc_wr(scc, SCC_CHANNEL_B, WR13, 0);
}

static bool scc_rx_ready(Z85C30 &scc) {
    return (scc.readCmd(SCC_CHANNEL_B) & 0x01) != 0;
}

static void test_scc_2400() {
    NClock clock;
    InterruptController irq;
    StubSerial dev;
    Z85C30 scc(&irq, &clock.c14m);
    scc.set_device_channel(SCC_CHANNEL_B, &dev);
    program_scc_2400(scc);

    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'A'}));
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'B'}));
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'C'}));
    scc.poll_modem_inputs();

    CHECK(clock.c14m.events().hasPendingEvents());
    uint64_t gap = clock.c14m.next_event().v - clock.get_c14m();
    CHECK(gap == cycles_8n1(2400.0f));
    CHECK(dev.q_dev.get_count() == 2);
    CHECK(!scc_rx_ready(scc));
    CHECK(!irq.get_irq(IRQ_ID_SCC));

    const uint8_t expect[3] = {'A', 'B', 'C'};
    for (int i = 0; i < 3; i++) {
        uint64_t queued = dev.q_dev.get_count();
        advance_just_before(clock);
        CHECK(!scc_rx_ready(scc));
        CHECK(dev.q_dev.get_count() == queued);
        advance_one(clock);
        CHECK(scc_rx_ready(scc));
        CHECK(irq.get_irq(IRQ_ID_SCC));
        CHECK((scc.readCmd(SCC_CHANNEL_A) & 0x04) != 0); /* RR3 channel B RX pending */
        CHECK(dev.q_dev.get_count() == queued);
        CHECK(scc.readData(SCC_CHANNEL_B) == expect[i]);
        CHECK(!scc_rx_ready(scc));
        if (i < 2) {
            CHECK(clock.c14m.events().hasPendingEvents());
            CHECK(clock.c14m.next_event().v - clock.get_c14m() == gap);
            CHECK(dev.q_dev.get_count() == queued - 1);
        }
    }
    CHECK(!clock.c14m.events().hasPendingEvents());
    CHECK(dev.q_dev.get_count() == 0);
}

static void test_scc_immediate_and_reset() {
    NClock clock;
    InterruptController irq;
    StubSerial dev;
    Z85C30 scc(&irq, &clock.c14m);
    scc.set_device_channel(SCC_CHANNEL_B, &dev);

    /* Reset state is x1, time constant 0: 921600, above MAX_TIMED_BAUD. */
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'Z'}));
    scc.poll_modem_inputs();
    CHECK(scc_rx_ready(scc));
    CHECK(!clock.c14m.events().hasPendingEvents());
    CHECK(scc.readData(SCC_CHANNEL_B) == 'Z');

    program_scc_2400(scc);
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'Q'}));
    scc.poll_modem_inputs();
    CHECK(clock.c14m.events().hasPendingEvents());
    CHECK(!scc_rx_ready(scc));
    uint64_t due = clock.c14m.next_event().v;
    scc.reset();
    CHECK(!clock.c14m.events().hasPendingEvents());
    uint64_t now = clock.get_c14m();
    if (due > now) {
        clock.adjust_c14m(due - now);
        clock.process_due();
    }
    CHECK(!scc_rx_ready(scc));
    CHECK(!clock.c14m.events().hasPendingEvents());
}

static void test_acia_2400() {
    NClock clock;
    InterruptController irq;
    StubSerial dev;
    MOS6551 acia(&irq, &clock.c14m, IRQ_SLOT_2);
    acia.set_device(&dev);
    acia.write_command(0x05); /* DTR, RTS, RX IRQ enabled */
    acia.write_control(0x1A); /* internal clock, 2400, 8N1 */

    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'A'}));
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'B'}));
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'C'}));
    acia.poll_modem_inputs();

    CHECK(clock.c14m.events().hasPendingEvents());
    uint64_t gap = clock.c14m.next_event().v - clock.get_c14m();
    CHECK(gap == cycles_8n1(2400.0f));
    CHECK(dev.q_dev.get_count() == 2);
    CHECK((acia.read_status() & 0x08) == 0);
    CHECK(!irq.get_irq(IRQ_SLOT_2));

    const uint8_t expect[3] = {'A', 'B', 'C'};
    for (int i = 0; i < 3; i++) {
        uint64_t queued = dev.q_dev.get_count();
        advance_just_before(clock);
        CHECK((acia.read_status() & 0x08) == 0);
        CHECK(dev.q_dev.get_count() == queued);
        advance_one(clock);
        CHECK(irq.get_irq(IRQ_SLOT_2));
        uint8_t st = acia.read_status();
        CHECK((st & 0x08) != 0);
        CHECK((st & 0x80) != 0);
        CHECK(dev.q_dev.get_count() == queued);
        CHECK(acia.read_data() == expect[i]);
        CHECK((acia.read_status() & 0x08) == 0);
        if (i < 2) {
            CHECK(clock.c14m.events().hasPendingEvents());
            CHECK(clock.c14m.next_event().v - clock.get_c14m() == gap);
            CHECK(dev.q_dev.get_count() == queued - 1);
        }
    }
    CHECK(!clock.c14m.events().hasPendingEvents());
    CHECK(dev.q_dev.get_count() == 0);
}

static void test_acia_immediate_and_reset() {
    NClock clock;
    InterruptController irq;
    StubSerial dev;
    MOS6551 acia(&irq, &clock.c14m, IRQ_SLOT_2);
    acia.set_device(&dev);
    acia.write_command(0x05);
    acia.write_control(0x00); /* external clock: baud 0, immediate */

    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'Z'}));
    acia.poll_modem_inputs();
    CHECK((acia.read_status() & 0x08) != 0);
    CHECK(!clock.c14m.events().hasPendingEvents());
    CHECK(acia.read_data() == 'Z');

    acia.write_control(0x1A);
    CHECK(dev.q_dev.send(SerialMessage{MESSAGE_DATA, 'Q'}));
    acia.poll_modem_inputs();
    CHECK(clock.c14m.events().hasPendingEvents());
    CHECK((acia.read_status() & 0x08) == 0);
    uint64_t due = clock.c14m.next_event().v;
    acia.reset();
    CHECK(!clock.c14m.events().hasPendingEvents());
    uint64_t now = clock.get_c14m();
    if (due > now) {
        clock.adjust_c14m(due - now);
        clock.process_due();
    }
    CHECK((acia.read_status() & 0x08) == 0);
    CHECK(!clock.c14m.events().hasPendingEvents());
}

int main() {
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    test_scc_2400();
    test_scc_immediate_and_reset();
    test_acia_2400();
    test_acia_immediate_and_reset();

    SDL_Quit();
    if (g_fails != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_fails);
        return 1;
    }
    std::printf("uartbaudtest: ok\n");
    return 0;
}
