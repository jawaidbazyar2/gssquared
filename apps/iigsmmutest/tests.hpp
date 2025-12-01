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
    std::string name;
    std::string description;
    std::vector<Operation> operations;
    
    Test(std::string n, std::string desc, std::initializer_list<Operation> ops)
        : name(std::move(n)), description(std::move(desc)), operations(ops) {}
};

// All tests
inline const std::vector<Test> ALL_TESTS = {
    // Test 1: Normal text page video shadowing
    Test{
        "test1",
        "Normal text page video shadowing",
        {
            WriteOp{0xe0c029, 0x01},
            WriteOp{0xe0c035, 0x08},
            WriteOp{0xe0c036, 0x84},
            WriteOp{0x000400, {0x12, 0x34}},
            //WriteOp{0x000401, 0x34},
            CopyOp{0xe00400, 0xe10400},
            AssertOp{0xe00400, 0x12},
            AssertOp{0xe00401, 0x34},
            WriteOp{0xe0c029, 0x01},

        }
    },
    
    // Add more tests here...
    // Test{
    //     "test2",
    //     "Another test description",
    //     {
    //         WriteOp{0x001000, {0xAA, 0xBB, 0xCC}},  // Multiple bytes use brackets
    //         AssertOp{0x001000, {0xAA, 0xBB, 0xCC}},
    //     }
    // },
};

} // namespace MMUTest
