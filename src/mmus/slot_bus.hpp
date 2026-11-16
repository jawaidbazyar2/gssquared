/*
 *   One access on the Apple II slot bus, passed from an MMU to a slot card's
 *   observer (MMU::set_bus_observer). The memory mapper creates this record
 *   before completing an access. The card may replace data on a read and
 *   return a delay through its observer. The mapper adds that delay to the
 *   Apple clock before continuing.
 *
 *   The record belongs to the caller: an observer that keeps it after its
 *   return must copy it.
 *
 *   On a IIgs the slot bus is the slow bus of the Mega II chip, and MMU_IIgs
 *   calls the observer. Source: Apple IIgs Technical Note 68, "The Mega II
 *   Select Signal", "The Mega II Bank 0 Signal" and "Using the Ready Signal".
 *   M2SEL is active low: zero means the address is valid on the slow bus.
 *   M2B0 is the original CPU bank bit, not the main/auxiliary RAM choice made
 *   by soft switches (addresses whose reads or writes change a hardware
 *   setting).
 *
 *   On an Apple II or IIe each CPU access is a bus cycle, and MMU_II calls
 *   the observer. These machines have neither signal: each cycle is selected,
 *   and the bank bit is zero.
 */

#pragma once

#include <cstdint>

// The slot bus clock PH0 on the 14.31818 MHz tick count. Tick zero is a
// scanline boundary. Each line has 65 bus periods: 64 of 14 ticks and a last
// one of 16, 912 ticks in all (Apple IIgs Hardware Reference, 2nd ed.,
// pp. 13-15, Fig. 2-2). PH0 falls at offsets 0, 14, ... 896 of a line.
inline uint64_t slot_bus_next_fall(uint64_t fall) {
    return fall + (fall % 912 == 896 ? 16 : 14);
}

// The first PH0 fall at or after an absolute tick.
inline uint64_t slot_bus_fall_at_or_after(uint64_t ticks) {
    const uint64_t line = ticks - ticks % 912;
    const uint64_t position = ticks % 912;
    return line + (position > 896 ? 912 : ((position + 13) / 14) * 14);
}

// The PH0 fall that ends a bus cycle the CPU starts at this tick. A CPU that
// is not on a PH0 fall waits for the next one first, as the IIgs FPI (Fast
// Processor Interface) does for a slow cycle.
inline uint64_t slot_bus_cycle_end(uint64_t start) {
    return slot_bus_next_fall(slot_bus_fall_at_or_after(start));
}

// Plain data shared at a host/card hand-off; no pointers into Apple memory.
struct slot_bus_transfer_t {
    // First physical data-phase fall, in absolute 14.31818 MHz host ticks.
    // RDY retries use the continuing 14/16-tick motherboard slot periods.
    uint64_t ticks = 0;
    // Low 16 address pins of this Apple access.
    uint16_t address = 0;
    // Write byte on entry; on reads the card may supply a replacement byte.
    uint8_t data = 0;
    // True for an Apple read, false for a write.
    bool read = true;
    // Address-phase bank bit as the card's slot gets it: bank $01/$E1 is
    // one, $00/$E0 is zero. Zero in a slot or machine without M2B0.
    bool m2b0 = false;
    // Electrical /M2SEL level. Default low means a selected slow-bus access.
    bool m2sel_n = false;
    // Set by the card when it supplies read data; otherwise normal RAM/I/O wins.
    bool driven = false;
    // A debugger access, not a bus cycle: ticks is unset, and the card may
    // only supply read data. It must not hold RDY or record an event.
    bool inspect = false;
};
