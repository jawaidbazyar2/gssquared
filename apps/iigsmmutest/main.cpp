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

    a->w("test%d:", testnum);
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
                for (auto byte : operation.data) {
                    a->w("        lda #%02X ", byte);
                    a->w("        sta $%02X/%04X", MMUTest::GetBank(operation.location), MMUTest::GetAddress(operation.location));
                }
            }
            else if constexpr (std::is_same_v<T, MMUTest::CopyOp>) {
                // Handle copy
                a->w(";  Copy from %02X/%04X to %02X/%04X\n",
                       MMUTest::GetBank(operation.source),
                       MMUTest::GetAddress(operation.source),
                       MMUTest::GetBank(operation.destination),
                       MMUTest::GetAddress(operation.destination));
                a->w("        lda $%02X/%04X",
                      MMUTest::GetBank(operation.source),
                      MMUTest::GetAddress(operation.source));
                a->w("        sta $%02X/%04X",
                      MMUTest::GetBank(operation.destination),
                      MMUTest::GetAddress(operation.destination));
            }
            else if constexpr (std::is_same_v<T, MMUTest::AssertOp>) {
                // Handle assert
                a->w(";  Assert %02X/%04X", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                a->w("        ldx #01");
                for (auto byte : operation.expected) {
                    a->w("        lda $%02X/%04X", MMUTest::GetBank(operation.location), MMUTest::GetAddress(operation.location));
                    a->w("        cmp #%02X ", byte);
                    a->w("        beq test%da%dgood", testnum, subtestnum);
                    a->w("        lda #%02X",testnum);
                    a->w("        jmp testfailed");
                    a->w("test%da%dgood:    inx", testnum, subtestnum);
                }
                subtestnum++;
            }
        }, op);
    }
}

void EmitAssemblyPreamble(AsmFile *a) {
    a->w("    .org $0800");
}
void EmitAssemblyPostamble(AsmFile *a) {
    a->w("testpassed   lda #00");
    a->w("testfailed   sta $00/07FE");
    a->w("        txa");
    a->w("        sta $00/07FF");
    a->w("        rts");    
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
