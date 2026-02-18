#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <variant>

#include "mmus/iigs_shadow_flags.hpp"

namespace MMUTest {

// Helper to extract bank and address from 32-bit value
// Format: 0x00BBAAAA where BB is bank, AAAA is address
constexpr uint8_t GetBank(uint32_t addr) { return (addr >> 16) & 0xFF; }
constexpr uint16_t GetAddress(uint32_t addr) { return addr & 0xFFFF; }

// Write operation: write data bytes to a bank/address
struct WriteOp {
    uint32_t location;  // Format: 0x00BBAAAA
    std::vector<uint8_t> data;
    
    // Single byte constructor
    WriteOp(uint32_t loc, uint8_t byte)
        : location(loc), data{byte} {}
    
    // Multiple bytes constructor
    WriteOp(uint32_t loc, std::initializer_list<uint8_t> bytes)
        : location(loc), data(bytes) {}
};

struct ReadOp {
    uint32_t location;  // Format: 0x00BBAAAA
    std::vector<uint8_t> expected;
    
    ReadOp(uint32_t loc, uint8_t byte)
        : location(loc), expected{byte} {}
    
    ReadOp(uint32_t loc, std::initializer_list<uint8_t> bytes)
        : location(loc), expected(bytes) {}
};

// Copy operation: copy from source to destination
struct CopyOp {
    uint32_t source;       // Format: 0x00BBAAAA
    uint32_t destination;  // Format: 0x00BBAAAA
    
    CopyOp(uint32_t src, uint32_t dst)
        : source(src), destination(dst) {}
};

// Assert operation: verify data at location matches expected values
struct AssertOp {
    uint32_t location;  // Format: 0x00BBAAAA
    std::vector<uint8_t> expected;
    
    // Single byte constructor
    AssertOp(uint32_t loc, uint8_t byte)
        : location(loc), expected{byte} {}
    
    // Multiple bytes constructor
    AssertOp(uint32_t loc, std::initializer_list<uint8_t> bytes)
        : location(loc), expected(bytes) {}
};

// Variant to hold any operation type
using Operation = std::variant<WriteOp, ReadOp, CopyOp, AssertOp>;

// A single test case
struct Test {
    int number;
    std::string description;
    std::vector<Operation> operations;
    
    Test(int n, std::string desc, std::initializer_list<Operation> ops)
        : number(n), description(std::move(desc)), operations(ops) {}
};

/**
 * All tests pass on a real Apple IIgs.
 */

inline const std::vector<Test> ALL_TESTS = {
    // Test 1: Normal text page video shadowing
    Test{
        1,
        "Normal text page video shadowing",
        {
            WriteOp{0xE0'c029, 0x01},
            WriteOp{0xE0'c035, 0x08},
            WriteOp{0xE0'c036, 0x84},
            WriteOp{0x00'0400, {0x12, 0x34}},
            AssertOp{0xE0'0400, {0x12, 0x34}},
            WriteOp{0xE0'c029, 0x01},

        }
    },
    Test{
        201,
        "text page 1 shadowing inhibited",
        {
            WriteOp{0xE0'0400, {0x00, 0x00}},
            WriteOp{0xE0'c029, 0x01},
            WriteOp{0xE0'c035, 0x08 | SHADOW_INH_TEXT1},
            WriteOp{0xE0'c036, 0x84},
            WriteOp{0x00'0400, {0x12, 0x34}},
            AssertOp{0xE0'0400, {0x00, 0x00}},
            WriteOp{0xE0'c029, 0x01},
            WriteOp{0xE0'c035, 0x08},
        }
    },
    Test{
        2,
        "shadow all banks copies data from any even bank to bank E0",
        {
            WriteOp{0xE0'C036, 0x94},
            WriteOp{0x00'0400, {0x12, 0x34}},
            WriteOp{0x02'0402, {0x56, 0x78}},
            WriteOp{0xE0'C036, 0x84},
            AssertOp{0xE0'0402, {0x56, 0x78}},
        }
    },
    Test{
        3,
        "shadow only copies data written to video pages",
        {
            WriteOp{0xE0'6000, {0xFF, 0xFF}},
            WriteOp{0x00'6000, {0x12, 0x34}},
            AssertOp{0xE0'6000, {0xFF, 0xFF}},
        }
    },
    Test{
        4,
        "write to text 1 with ramwrt=1 shadowed to e1",
        {
            WriteOp{0xE0'0400, {0x00, 0x00}},
            WriteOp{0xE1'0400, {0x00, 0x00}},
            WriteOp{0xE0'C005, 0x01},
            WriteOp{0x00'0400, {0x56, 0x78}},
            WriteOp{0xE0'C004, 0x01},
            AssertOp{0xE1'0400, {0x56, 0x78}},
            AssertOp{0xE0'0400, {0x00, 0x00}},
        }
    },
    Test{
        5,
        "aux write (non-video) put in aux but not shadowed to e1",
        {
            WriteOp{0x01'6000, {0x00, 0x00}},            
            WriteOp{0xE1'6000, {0x00, 0x00}},            
            WriteOp{0xE0'C005, 0x01},
            WriteOp{0x00'6000, {0x56, 0x78}},
            WriteOp{0xE0'C004, 0x01},
            AssertOp{0xE1'6000, {0x00, 0x00}},
            AssertOp{0x01'6000, {0x56, 0x78}},
        }
    },
    Test{
        6,
        "bank 2 + aux write (non-video) stored in 'aux' and not shadowed to e1 (all banks shadow)",
        {
            WriteOp{0xE1'6000, {0x00, 0x00}},
            WriteOp{0x02'6000, {0x00, 0x00}},
            WriteOp{0x03'6000, {0x00, 0x00}},
            
            WriteOp{0xE0'C036, 0x94},
            WriteOp{0xE0'C005, 0x01},
            WriteOp{0x02'6000, {0x56, 0x78}},
            WriteOp{0xE0'C004, 0x01},           
            WriteOp{0xE0'C036, 0x84},
           
            AssertOp(0xE1'6000, {0x00, 0x00}),
            AssertOp(0x02'6000, {0x00, 0x00}),
            AssertOp(0x03'6000, {0x56, 0x78}),
        }
    },
    Test{
        7,
        "aux write shadowed to e1 - direct access to aux bank",
        {
            WriteOp{0xE1'0400, {0x00,0x00}},
            WriteOp{0x01'0400, {0x56, 0x78}},
            AssertOp{0xE1'0400, {0x56, 0x78}},
        }
    },
    Test{
        8,
        "aux write to odd bank 3 with all bank shadow enabled",
        {
            WriteOp{0xE1'0400, {0x00,0x00}}, // clear
            WriteOp{0xE0'c036, 0x94}, // enable all bank shadow
            WriteOp{0x03'0400, {0x56, 0x78}},
            WriteOp{0x03'C030, 0x00}, // set bank 3
            WriteOp{0xE0'c036, 0x84}, // disable all bank shadow
            AssertOp{0xE1'0400, {0x56, 0x78}},
        }
    },
    Test{
        9,
        "IOLC inhibit disables access to CXXX in bank 0",
        {
            WriteOp{0xE0'0400, 0x00},
            WriteOp{0xE0'c035, 0x68}, // disable IOLC; disable Text Page 2; disable SHR;
            WriteOp{0x00'C010, 0x12},
            CopyOp{0x00'C010, 0xE0'0400},
            WriteOp{0xE0'c035, 0x28}, // enable IOLC; disable Text Page 2; disable SHR;
            AssertOp{0xE0'0400, 0x12},
        }
    },
    Test{
        10,
        "there is normally no IOLC in bank 2",
        {
            WriteOp{0x02'C010, 0x12}, // normally, this should be RAM.
            AssertOp{0x02'C010, 0x12},
        }
    },
    Test{
        11,
        "there is an IOLC in bank 2 with ALL BANK SHADOW enabled",
        {
            WriteOp{0x02'C010, 0x00}, // currently RAM.
            WriteOp{0xE0'c036, 0x94}, // enable all bank shadow
            WriteOp{0x02'C010, 0x12}, // this should now be IO, and our write should not go to RAM.
            WriteOp{0xE0'c036, 0x84}, // disable all bank shadow
            AssertOp{0x02'C010, 0x00}, // this should be unchanged since write occurred to IO.
        }
    },
    Test{
        12,
        "bank latch disabled, write to E1 goes to E0 instead",
        {
            WriteOp{0xE0'6000, {0x00, 0x00}},
            WriteOp{0xE1'6000, {0x00, 0x00}},
            WriteOp{0xE0'C029, 0x00}, // disable bank latch
            WriteOp{0xE1'6000, {0x12, 0x34}},
            WriteOp{0xE0'C029, 0x01}, // reenable bank latch
            AssertOp(0xE0'6000, {0x12, 0x34}), // write should have gone to E0.
            AssertOp(0xE1'6000, {0x00, 0x00}),
        }
    },
    Test{
        13,
        "bank latch disabled; write to E0 goes to E1 with RAMWRT set",
        {
            WriteOp{0xE0'6100, { 0x00, 0x00}},
            WriteOp{0xE1'6100, { 0x00, 0x00}},

            WriteOp{0xE0'C029, 0x00}, // disable bank latch
            WriteOp{0xE0'C005, 0x01},
            WriteOp{0xE0'6100, { 0x12, 0x34 }},
            WriteOp{0xE0'C004, 0x01},
            WriteOp{0xE0'C029, 0x01}, // reenable bank latch
            AssertOp{ 0xE0'6100, { 0x00, 0x00}},
            AssertOp{ 0xE1'6100, { 0x12, 0x34}},
        }
    },
    Test{
        14,
        "bank latch disabled, all bank shadowing enabled, write to 03 goes to 03",
        {
            WriteOp{0x02'6200, { 0x00, 0x00}},
            WriteOp{0x03'6200, { 0x00, 0x00}},
            WriteOp{0xE0'C036, 0x94}, // enable all bank shadow
            WriteOp{0xE0'C029, 0x00}, // disable bank latch

            WriteOp{0x03'6200, { 0x12, 0x34}},

            WriteOp{0xE0'C029, 0x01}, // reenable bank latch
            WriteOp{0xE0'C036, 0x84}, // disable all bank shadow

            AssertOp{0x02'6200, { 0x00, 0x00}},
            AssertOp{0x03'6200, { 0x12, 0x34}},            
        }
    },
    Test{
        15,
        "IOLC inhibit does not disable other IIe memory management: aux write",
        {          
            WriteOp{0x00'6400, {0x00, 0x00}}, // clear test areas
            WriteOp{0x01'6400, {0x00, 0x00}},
            WriteOp{0xE0'6400, {0x00, 0x00}},
            WriteOp{0xE1'6400, {0x00, 0x00}},

            WriteOp{0xE0'C035, 0x68}, // disable IOLC; disable Text Page 2; disable SHR;
            WriteOp{0xE0'C005, 0x01}, // enable RAMWRT
            WriteOp{0x00'6400, {0x56, 0x78}}, // should go to 01/6400
            WriteOp{0xE0'6401, 0x89}, // would go to E0/6001; but RAMWRT is on so it goes to E1/6001
            WriteOp{0xE0'C004, 0x01}, // disable RAMWRT
            WriteOp{0xE0'C035, 0x28}, // enable IOLC; disable Text Page 2; disable SHR;

            AssertOp{0x00'6400, {0x00, 0x00}}, // no change here
            AssertOp{0x01'6400, {0x56, 0x78}}, // 00 writes should go here
            AssertOp{0xE0'6400, {0x00, 0x00}}, // but the E0 write
            AssertOp{0xE1'6400, {0x00, 0x89}}, // should go here
        }
    },
    Test{ // NO. But it does with all bank shadow enabled (see test 6)
        16,
        "RAMWRT has no effect in banks 02/03 without all bank shadow",
        {
            WriteOp{0xE1'6500, {0x00, 0x00}},
            WriteOp{0x02'6500, {0x00, 0x00}},
            WriteOp{0x03'6500, {0x00, 0x00}},
            
            WriteOp{0xE0'C005, 0x01},
            WriteOp{0x02'6500, {0x56, 0x78}},
            WriteOp{0xE0'C004, 0x01},           
           
            AssertOp(0xE1'6500, {0x00, 0x00}),
            AssertOp(0x02'6500, {0x56, 0x78}),
            AssertOp(0x03'6500, {0x00, 0x00}),
        }
    },
    Test{
        17,
        "main LC Bank $00 (LC Bank 2)",
        {
            ReadOp{0xE0'C082, 0x00}, // make sure LC RAM is disabled for r and w
            ReadOp{0xE0'C083, 0x00}, // hit once to enable for read
            ReadOp{0xE0'C083, 0x00}, // hit again to enable for write
            WriteOp{0x00'D000, 0x12}, // write to LC RAM
            WriteOp{0x00'E000, 0x34},
            AssertOp{0x00'D000,0x12}, // if we didn't have RAM, this would be some value from ROM and fail this test.
            AssertOp{0x00'E000,0x34},
            ReadOp{0xE0'C082, 0x00},   // disable ram r/w again. (for later)         
        }
    },
    Test{
        18,
        "main LC Bank $E0 (LC Bank 2)",
        {
            ReadOp{0xE0'C082, 0x00}, // make sure LC RAM is disabled for r and w
            ReadOp{0xE0'C083, 0x00}, // hit once to enable for read
            ReadOp{0xE0'C083, 0x00}, // hit again to enable for write
            WriteOp{0xE0'D000, 0x12}, // write to LC RAM
            WriteOp{0xE0'E000, 0x34},
            AssertOp{0xE0'D000,0x12}, // if we didn't have RAM, this would be some value from ROM and fail this test.
            AssertOp{0xE0'E000,0x34},
            ReadOp{0xE0'C082, 0x00},   // disable ram r/w again. (for later)         
        }
    },
    Test{
        19,
        "main LC Bank $00 (LC Bank 2) does not shadow to $E0",
        {
            ReadOp{0xE0'C082, 0x00}, // make sure LC RAM is disabled for r and w
            ReadOp{0xE0'C083, 0x00}, // hit once to enable for read
            ReadOp{0xE0'C083, 0x00}, // hit again to enable for write
            WriteOp{0xE0'E000, 0x00}, // write to LC RAM
            WriteOp{0x00'E000, 0x34},
            AssertOp{0xE0'E000,0x00},
            ReadOp{0xE0'C082, 0x00},   // disable ram r/w again. (for later)         
        }
    },
    /* This used to do C068:68 then C068:28, but that includes ROM3 "inhibit text page 2 shadow". 
       Have separate test for shadow and ROM03 */
    Test{
        20,
        "main LC bank $00 (LC Bank 1) write $D000, disable IOLC shadow, readable at $C000",
        {
            ReadOp(0xE0'C08B,0x00), // make LC bank 1 ram r/w
            ReadOp(0xE0'C08B,0x00), // make LC bank 1 ram r/w
            WriteOp(0x00'D000,0x12), // write to LC bank 1 ram
            ReadOp(0xE0'C08A,0x00), // set back to ROM
            WriteOp(0xE0'C035,0x48), // inhibit IOLC shadow
            AssertOp(0x00'C000,0x12), // read from flat bank 0 ram
            WriteOp(0xE0'C035,0x08), // re-enable IOLC shadow
        }
    },
    Test{
        21,
        "ROM appears in bank FF",
        {
            AssertOp{0xFF'D000, 0x6f},
            AssertOp{0xFF'E000, 0x4C},
        }
    },
    Test{
        22,
        "ROM appears in bank 00 shadowed",
        {
            AssertOp{0x00'D000, 0x6f},
            AssertOp{0x00'E000, 0x4C},
        }
    },

    // TODO: verify C071-C07F are present when IOLC enabled
    Test{
        23,
        "C071-C07F are present when IOLC enabled",
        {
            //WriteOp{0xE0'C035, 0x68}, // inhibit IOLC shadow
            AssertOp{0x00'C071, {0xE2, 0x40, 0x50, 0xB8}},      
        }
    },
    /* These are tests against a specific bug I had in iigs_mmu that caused the aux bank offset to be triggered
       anytime LC_BANK2 was enabled. */
    Test{
        24, 
        "LC Bank 2 Does Not Trigger aux_read 0x1'0000 offset - calc_aux_write",
        {
            WriteOp{0x00'0036, {0x12, 0x34}},
            WriteOp{0xE0'C068, 0x0C},
            AssertOp{0x00'0036, {0x12, 0x34}},
            WriteOp{0xE0'C068, 0x08},
        }
    },
    Test{
        25, 
        "LC Bank 2 Does Not Trigger aux_read 0x1'0000 offset - calc_aux_write",
        {
            WriteOp{0xE0'C068, 0x0C},
            WriteOp{0x00'0036, {0x12, 0x34}},
            WriteOp{0xE0'C068, 0x08},
            AssertOp{0x00'0036, {0x12, 0x34}},
        }
    },

    // Test: RAMWRT + Bank 1 Write - should write to bank 1, not bank 2.
    Test{
        26, 
        "RAMWRT + Bank 1 Write - should write to bank  still, not bank 2.",
        {
            WriteOp(0x01'1666, {0xEA, 0xEA}),
            WriteOp(0x02'1666, {0xEA, 0xEA}),
            WriteOp{0xE0'C068, 0x1C},
            WriteOp(0x01'1666, {0x12, 0x34}),
            WriteOp{0xE0'C068, 0x0C},
            AssertOp{0x01'1666, {0x12, 0x34}}, // test this changed
            AssertOp{0x02'1666, {0xEA, 0xEA}}, // and this didn't.
        }
    },

    // Test: when iOLC Shadow not inhibited, and bank latch enabled, I/O Space appears in all banks 00, 01, E0, E1
    // E1: verifies I/O operation with bank latch enabled.
    Test{
        27,
        "when iOLC not inhibited, I/O Space appears in all banks 00, 01, E0, E1",
        {
            AssertOp{0x00'C200, {0xE2, 0x40}},
            AssertOp{0x01'C200, {0xE2, 0x40}},
            AssertOp{0xE0'C200, {0xE2, 0x40}},
            AssertOp{0xE1'C200, {0xE2, 0x40}},
        }
    },
    // Test: when IOLC not inhibited, interrupt vector pull reads from ROM.
    // Test: when IOLC inhibited, interrupt vector pull reads from RAM.
    // Test: when LC RAM READ enabled, bit 3 in State tracks.
    // Test: when bit 3 in State is changed, LC RAM READ ENABLE tracks.
    // Test: ramwrt does not affect ZP/Stack
    // Test: altzp affects ZP/Stack
    // Test: LC Bank 1/2 and LCBNK2 sense are in sync and select correct RAM
    // Test: RAMWRT + 80STORE + PAGE1 - should reference main memory.
    
    Test{
        0x99,
        "floating bus behavior: ram bank higher than real RAM data reads as the bank number",
        {
            AssertOp{0x81'0000, 0x81},
            AssertOp{0x82'0000, 0x82},
        }
    },

    // not a test: just set memory state registers back to known good values.
    // C068: 0C
    // C035: ??

    // TODO: verify C000 operates in bank E1
    // TODO: verify C011-C01F "read switch status" works
    // TODO: test C068 State Register

};

} // namespace MMUTest
