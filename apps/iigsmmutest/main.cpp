#include <cstdio>
#include "AsmFile.hpp"
#include "tests.hpp"

/*

Given input test data, perform one of several functions:

1. Emit 65816 assembly code to perform the tests;
2. execute the tests using the IIgs MMU module.

*/

void displayTest(const MMUTest::Test& test) {
    printf("Running: %s - %s\n", test.name.c_str(), test.description.c_str());
    
    for (const auto& op : test.operations) {
        std::visit([](auto&& operation) {
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, MMUTest::WriteOp>) {
                // Handle write
                printf("  Write to %02X/%04X: ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                for (auto byte : operation.data) printf("%02X ", byte);
                printf("\n");
            }
            else if constexpr (std::is_same_v<T, MMUTest::CopyOp>) {
                // Handle copy
                printf("  Copy from %02X/%04X to %02X/%04X\n",
                       MMUTest::GetBank(operation.source),
                       MMUTest::GetAddress(operation.source),
                       MMUTest::GetBank(operation.destination),
                       MMUTest::GetAddress(operation.destination));
            }
            else if constexpr (std::is_same_v<T, MMUTest::AssertOp>) {
                // Handle assert
                printf("  Assert %02X/%04X == ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                for (auto byte : operation.expected) printf("%02X ", byte);
                printf("\n");
            }
        }, op);
    }
}

void EmitTestAssembly(int testnum, const MMUTest::Test& test, AsmFile *a) {
    a->w("; Running: %s - %s\n", test.name.c_str(), test.description.c_str());

    a->w("test%d", testnum);
    a->w("        ldx #00");
    int subtestnum = 1;

    for (const auto& op : test.operations) {
        std::visit([a,testnum,&subtestnum](auto&& operation) {
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, MMUTest::WriteOp>) {
                // Handle write
                a->w(";  Write to %02X/%04X: ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location) );
                int bytecount = 0;
                for (auto byte : operation.data) {
                    a->w("        lda #$%02X ", byte);
                    a->w("        sta >$%02X%04X", MMUTest::GetBank(operation.location), MMUTest::GetAddress(operation.location)+bytecount);
                    bytecount++;
                }
            }
            else if constexpr (std::is_same_v<T, MMUTest::CopyOp>) {
                // Handle copy
                a->w(";  Copy from %02X/%04X to %02X/%04X\n",
                       MMUTest::GetBank(operation.source),
                       MMUTest::GetAddress(operation.source),
                       MMUTest::GetBank(operation.destination),
                       MMUTest::GetAddress(operation.destination));
                a->w("        lda >$%02X%04X",
                      MMUTest::GetBank(operation.source),
                      MMUTest::GetAddress(operation.source));
                a->w("        sta >$%02X%04X",
                      MMUTest::GetBank(operation.destination),
                      MMUTest::GetAddress(operation.destination));
            }
            else if constexpr (std::is_same_v<T, MMUTest::AssertOp>) {
                // Handle assert
                a->w(";  Assert %02X/%04X", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                a->w("        ldx #01");
                int bytecount = 0;
                for (auto byte : operation.expected) {
                    a->w("        lda >$%02X%04X", MMUTest::GetBank(operation.location), MMUTest::GetAddress(operation.location)+bytecount);
                    a->w("        cmp #$%02X ", byte);
                    a->w("        beq test%da%db%dok", testnum, subtestnum, bytecount);
                    a->w("        lda #$%02X",testnum);
                    a->w("        jsr recordfail");
                    a->w("test%da%db%dok", testnum, subtestnum, bytecount);
                    bytecount++;
                }
                a->w("test%da%dgood    inx", testnum, subtestnum);
                subtestnum++;
            }
        }, op);
    }
}

void EmitAssemblyPreamble(AsmFile *a) {
    std::vector<std::string> testnames = {
        "        org $0800",
        "        ldy #00",
        "        lda #00",
        };
    for (const auto& testname : testnames) {
        a->w(testname.c_str());
    }
}

void EmitAssemblyPostamble(AsmFile *a) {
    std::vector<std::string> testnames = {
    "testpassed   lda #00",
    "testfailed   cpy #00",
    "        beq exittest",
    "        jsr displayfail",
    "exittest     rts",

    "recordfail",
    "        sta failtable,Y",  // test number
    "        iny",
    "        txa",
    "        sta failtable,Y",  // subtest number
    "        iny",
    "        rts",
    "displayfail",
    "        tyx",
    "        ldy #00",
    "dfloop",
    "        lda failtable,Y",
    "        jsr $FDDA",
    "        iny",
    "        lda failtable,Y",
    "        jsr $FDDA",
    "        iny",
    "        lda #$8D",
    "        jsr $FDED",
    "        dex",
    "        dex",
    "        bne dfloop",
    "        rts",
    "failtable ds 256",
    };
    for (const auto& testname : testnames) {
        a->w(testname.c_str());
    }
}

int main(int argc, char *argv[]) 
{
    printf("Starting IIgs MMU test...\n\n");
    
    for (const auto& test : MMUTest::ALL_TESTS) {
        displayTest(test);
        printf("\n");
    }

    AsmFile *a = new AsmFile("test.asm");
    EmitAssemblyPreamble(a);
    int testnum = 1;
    for (const auto& test : MMUTest::ALL_TESTS) {
        EmitTestAssembly(testnum, test, a);
        testnum++;
    }
    EmitAssemblyPostamble(a);
    delete a;

    return 0;
}
