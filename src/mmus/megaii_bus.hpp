/*
 *   One Apple IIgs expansion-bus access, passed from MMU_IIgs to Second Sight.
 *   Mega II is the chip that supplies the slow Apple II memory and slot bus.
 *   The memory mapper creates this record before completing an access. The
 *   card may replace data on a read and return a delay through its observer.
 *   The mapper adds that delay to the Apple clock before continuing.
 *
 *   The record belongs to the caller. A queued card job receives a copy;
 *   a synchronous read borrows it only until the worker has finished.
 *   Source: Apple IIgs Technical Note 68, "The Mega II Select Signal", "The
 *   Mega II Bank 0 Signal" and "Using the Ready Signal". M2SEL is active low:
 *   zero means the address is valid on the slow bus. M2B0 is the original
 *   CPU bank bit, not the main/auxiliary RAM choice made by soft switches
 *   (addresses whose reads or writes change a hardware setting).
 */

#pragma once

#include <cstdint>

// Plain data shared at a host/card hand-off; no pointers into IIgs memory.
struct megaii_transfer_t {
    // First physical data-phase fall, in absolute 14.31818 MHz host ticks.
    // RDY retries use the continuing 14/16-tick motherboard slot periods.
    uint64_t ticks = 0;
    // Low 16 address pins of this Apple access.
    uint16_t address = 0;
    // Write byte on entry; on reads the card may supply a replacement byte.
    uint8_t data = 0;
    // True for an Apple read, false for a write.
    bool read = true;
    // Address-phase bank bit: bank $01/$E1 is one, $00/$E0 is zero.
    bool m2b0 = false;
    // Electrical /M2SEL level. Default low means a selected slow-bus access.
    bool m2sel_n = false;
    // Set by the card when it supplies read data; otherwise normal RAM/I/O wins.
    bool driven = false;
};
