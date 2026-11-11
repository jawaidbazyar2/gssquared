/*
 *   Second Sight clock and bus-order checks using small in-memory Z180 programs.
 *   SSCard receives Apple accesses and runs until each supplied host deadline;
 *   assertions inspect the resulting registers, copied bytes and latch edges.
 *   Each fixture owns its card on the calling thread and waits before reading it.
 *
 *   PHI is the Z180 processor clock; a T-state lasts one PHI period and a half-
 *   PHI is half that period. Mega II supplies the Apple IIgs slow bus. Most
 *   fixtures deliberately use an 18 MHz host tick rate for simple arithmetic,
 *   not the real IIgs 14.31818 MHz clock. DMA transfers bytes without executing
 *   CPU instructions; DREQ requests one DMA channel. A latch retains a byte.
 *   IORQ selects an I/O port; ACK acknowledges the pending Apple event. HD is
 *   the card data bus. IM0 is interrupt mode zero, which executes a bus opcode.
 *
 *   Sources: Zilog UM005004-0918 pp. 16-30 (bus edges), 34-41 (clock control),
 *   73-90 (interrupts), 95-113 (DMA), and ROM_BEEF.asm ClearScreen at
 *   https://github.com/jawaidbazyar2/secondsight .
 */

// External deadlines stay in real time when firmware changes CCR.
#include "devices/secondsight/ss_card.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>

// Run the independent fixtures below; assertions fail at the violated contract.
// No arguments are used. A normal return means every check completed.
int main() {
    // U18 Z8S180 processor pin 50 (/DREQ0) is grounded. Fixed-source DMA needs the asserted
    // request even in burst mode; ClearScreen uses it before SetMode.
    for (bool warm : {false, true}) {
        std::array<uint8_t, 0x20000> rom{};
        rom[0] = 0x76;
        SSCard card(rom.data(), 18'000'000);
        if (warm) card.reset(1800);
        auto &z = card.processor();
        // DCNTL ($32) removes programmed waits; RCR ($36) disables refresh
        // so this fixture isolates the requested DMA or instruction timing.
        z.write_register(0x32, 0);
        z.write_register(0x36, 0);
        card.memory_write(0x20100, 0x5a);
        for (unsigned a = 0x201ff; a <= 0x20204; ++a)
            card.memory_write(a, 0xcc);
        // SAR0/DAR0/BCR0 ($20-$27): source $20100, destination $20200,
        // four bytes. The surrounding $CC bytes detect an extra DMA store.
        const uint8_t registers[] = {0, 1, 2, 0, 2, 2, 4, 0};
        for (unsigned i = 0; i < sizeof(registers); ++i)
            z.write_register(0x20 + i, registers[i]);
        z.write_register(0x31, 0x0a); // Fixed memory source, incrementing destination, burst.
        z.write_register(0x30, 0x41);
        card.catch_up(warm ? 3600 : 1800);
        assert(z.read_register(0x26) == 0 && z.read_register(0x27) == 0);
        assert(!(z.read_register(0x30) & 0x40));
        assert(card.sram_data()[0x1ff] == 0xcc && card.sram_data()[0x204] == 0xcc);
        for (unsigned a = 0x200; a < 0x204; ++a)
            assert(card.sram_data()[a] == 0x5a);
    }
    std::puts("PASS grounded DREQ0: fixed-source DMA fill and terminal count after cold/warm reset");
    // CCR ($1F) bit 7 removes the divide-by-two clock. Changing it mid-run
    // must preserve the same external deadline in emulated seconds.
    for (bool slow : {false, true}) {
        std::array<uint8_t, 0x20000> rom{};
        // LD A,n / OUT0 (CCR),A / HALT
        const uint8_t code[] = {0x3e, uint8_t(slow ? 0 : 0x80), 0xed, 0x39, 0x1f, 0x76};
        std::copy(std::begin(code), std::end(code), rom.begin());
        SSCard card(rom.data(), 18'000'000);
        card.processor().write_register(0x32, 0); // No DCNTL waits in this fixture.
        if (slow)
            card.processor().write_register(0x1f, 0x80);
        card.catch_up(1800); // 100 microseconds, including the clock-changing instruction.
        const auto cycles = card.processor().cycles();
        assert(card.processor().halted());
        if (slow)
            assert(cycles >= 1800 && cycles < 1850);
        else
            assert(cycles > 3500 && cycles <= 3600);
        card.catch_up(3600);
        const auto delta = card.processor().cycles() - cycles;
        assert(delta >= (slow ? 1799 : 3599) && delta <= (slow ? 1801 : 3601));
    }
    // Cross VGA deadlines and the no-event sentinel using the same integer
    // conversion as the card. Rounding down would dispatch a raster deadline before its time.
    assert(Z180Time::from_seconds(std::numeric_limits<double>::infinity()).is_never());
    assert(Z180Time::from_seconds(0.58568769442804524683) > Z180Time::from_ticks(21084757, 36'000'000));
    std::array<uint8_t, 0x20000> halted_rom{};
    halted_rom[0] = 0x76;
    SSCard idle(halted_rom.data(), 18'000'000);
    idle.processor().write_register(0x36, 0); // Keep this scheduler regression short.
    idle.catch_up(18'000'000);
    assert(idle.processor().cycles() >= 18'000'000);
    // HD remembers only electrically driven transfers; internal readback
    // and unselected Oak ports cannot replace it with a synthetic FF.
    std::array<uint8_t, 0x20000> bus_rom{};
    bus_rom[0] = 0xed;
    bus_rom[1] = 0x38;
    bus_rom[2] = 0x30; // IN0 A,(DSTAT)
    bus_rom[3] = 0xed;
    bus_rom[4] = 0x39;
    bus_rom[5] = 6; // OUT0 (TDR0),A
    SSCard bus_card(bus_rom.data(), 18'000'000);
    assert(!bus_card.data_bus_driven() && bus_card.interrupt_bus_byte() == 0xff);
    assert(bus_card.io_read(0x30) == 0xff && !bus_card.data_bus_driven());
    bus_card.processor().write_register(0x32, 0);
    bus_card.processor().write_register(0x36, 0);
    bus_card.processor().step();
    assert(bus_card.processor().get_state().af >> 8 == 0x32);
    assert(bus_card.data_bus_driven() && bus_card.interrupt_bus_byte() == 0x30);
    auto bs = bus_card.processor().get_state();
    bs.af = 0x9900;
    bus_card.processor().set_state(bs);
    bus_card.processor().step();
    assert(bus_card.interrupt_bus_byte() == 0x99);
    assert(bus_card.io_read(0x30) == 0x99);
    bus_card.memory_write(0x60000, 0x42);          // ROM ignores write; CPU still drives HD.
    assert(bus_card.memory_read(0x40000) == 0x42); // Half-select has no driver.
    bus_card.reset(0);
    assert(bus_card.data_bus_driven() && bus_card.interrupt_bus_byte() == 0x42);
    // IM0 samples the held byte at ACK, without an opcode PC increment.
    for (uint8_t byte : {uint8_t(0), uint8_t(0xff)}) {
        std::array<uint8_t, 0x20000> irq_rom{};
        SSCard irq_card(irq_rom.data(), 18'000'000);
        auto &z = irq_card.processor();
        // DCNTL ($32) removes programmed waits; RCR ($36) disables refresh
        // so this fixture isolates the requested DMA or instruction timing.
        z.write_register(0x32, 0);
        z.write_register(0x36, 0);
        auto st = z.get_state();
        st.iff1 = 1;
        st.sp = 0x4000;
        z.set_state(st);
        z.set_input(Z180::Input::INT0, true);
        z.step(); // NOP finishes the eligible CPU interrupt sample.
        irq_card.memory_write(0x60000, byte);
        z.step();
        assert(z.get_state().pc == (byte ? 0x38 : 1));
        assert(z.get_state().sp == (byte ? 0x3ffe : 0x4000));
    }
    // ACK asserts at IORQ select; U3 Apple data-out latch loads at the final write-data edge.
    // A host access between them must neither read stale U3 Apple data-out latch nor replace U2 Apple data-in latch
    // before an A0 read samples it. Exercise both OMCR.IOC select edges.
    // Vary read/write, IOC (I/O select-edge control) and relocated internal
    // registers. Internal reads do not supply the external latch byte, so
    // that combination is deliberately excluded (UM0050 p. 44).
    for (bool read : {false, true})
    for (bool early : {false, true})
    for (bool internal : {false, true}) {
        if (read && internal)
            continue;
        std::array<uint8_t, 0x20000> rom{};
        const uint8_t code[] = {0x3e, 0x5a, 0xed, uint8_t(read ? 0x38 : 0x39), 0xa0, 0x76};
        std::copy(std::begin(code), std::end(code), rom.begin());
        SSCard card(rom.data(), 36'000'000); // One host tick per half-PHI for this edge fixture.
        auto &z = card.processor();
        // DCNTL ($32) removes programmed waits; RCR ($36) disables refresh
        // so this fixture isolates the requested DMA or instruction timing.
        z.write_register(0x32, 0);
        z.write_register(0x36, 0);
        z.write_register(0x3e, 0xc0 | (early ? 0x20 : 0));
        if (internal)
            z.write_register(0x3f, 0x80); // Internal CBR at A0 still asserts the board's write pins.
        card.io_write(0xa0, 0xa5);
        slot_bus_transfer_t command{0, 0xc0b0, 0x35, false};
        card.observe(command);
        assert(card.capture_state().pending);
        unsigned select = 0, data = 0;
        uint64_t data_half = 0;
        z.set_cycle_observer([&](uint64_t half, uint8_t, uint8_t edge, uint32_t address, uint8_t value) {
            if (address != 0xa0)
                return;
            if (edge == (early ? 1 : 2)) {
                ++select;
                assert(!card.capture_state().pending);
                assert(card.output_latch() == 0xa5);
            }
            if (edge == 4) {
                ++data;
                data_half = half;
                assert(value == (read ? 0x35 : 0x5a));
                assert(card.output_latch() == (read ? 0xa5 : 0x5a));
            }
        });
        // Stop between select and data edges, with a finite guard so a
        // missing select reaches the assertion instead of hanging the fixture.
        while (!select && z.half_cycles() < 100)
            z.run_half_until(z.half_cycles() + 1);
        assert(select == 1 && data == 0);
        // Reads fetch the old command; writes deliver the next result byte.
        const uint64_t fall = ((z.half_cycles() + 13) / 14) * 14;
        slot_bus_transfer_t next{fall, 0xc0b2, 0x99, !read};
        assert(card.observe(next) == 0);
        assert(select == 1 && data == 1);
        // The Apple samples on PH0 fall, not on the end of the A0 pulse.
        // Here that fall follows the data edge (Apple IIgs Technical Note 68,
        // Fig. 2; Zilog UM0050, Figs. 7-8).
        assert(data_half <= fall && z.half_cycles() == fall);
        assert(card.capture_state().pending); // The new host event survives the A0 cycle.
        if (read)
            assert(card.capture_state().host_input == 0x99);
        else
            assert(next.driven && next.data == 0x5a);
    }
    std::puts("PASS A0 select/data edges, Apple data latch handoff, next event and relocated internal writes in both IOC modes");
    std::puts("PASS HD cold FF, direct-fetch drive, internal I/O, warm reset and held-byte IM0");
    std::puts("PASS CCR speed-up/slow-down, instruction-boundary yield and HALT deadlines");
}
