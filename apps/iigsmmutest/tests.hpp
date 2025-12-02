#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <variant>

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
using Operation = std::variant<WriteOp, CopyOp, AssertOp>;

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
            AssertOp{0xE0'0400, 0x12},
            AssertOp{0xE0'0401, 0x34},
            WriteOp{0xE0'c029, 0x01},

        }
    },
    Test{
        2,
        "shadow all banks copies data from any even bank to bank E0",
        {
            WriteOp{0xE0'C036, 0x94},
            WriteOp{0x00'0400, {0x12, 0x34}},
            WriteOp{0x02'0402, {0x56, 0x78}},
            WriteOp{0xE0'C036, 0x94},
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
        0x99,
        "floating bus behavior: ram bank higher than real RAM data reads as the bank number",
        {
            AssertOp{0x81'0000, 0x81},
            AssertOp{0x82'0000, 0x82},
        }
    }

};

} // namespace MMUTest
