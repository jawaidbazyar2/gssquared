#include <cstdio>
#include <unistd.h>
#include "AsmFile.hpp"
#include "tests.hpp"

#include "mmus/mmu_iigs.hpp"
#include "mmus/mmu_iie.hpp"

uint64_t debug_level = 0;

/*

Given input test data, perform one of several functions:

1. Emit 65816 assembly code to perform the tests;
2. execute the tests using the IIgs MMU module.

*/

void displayTest(const MMUTest::Test& test) {
    int testnum = test.number;
    printf("Running: %d - %s\n", testnum, test.description.c_str());
    
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
            else if constexpr (std::is_same_v<T, MMUTest::ReadOp>) {
                // Handle read
                printf("  Read from %02X/%04X: ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                //for (auto byte : operation.expected) printf("%02X ", byte);
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

void EmitTestAssembly(const MMUTest::Test& test, AsmFile *a) {
    int testnum = test.number;

    a->w("; Running: %d - %s\n", test.number, test.description.c_str());

    a->w("test%d", testnum);
    a->w("        ldx #01");
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
            else if constexpr (std::is_same_v<T, MMUTest::ReadOp>) {
                // Handle read
                a->w(";  Read from %02X/%04X\n",
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                a->w("        lda >$%02X%04X", MMUTest::GetBank(operation.location), MMUTest::GetAddress(operation.location));
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
                //a->w("        ldx #01");
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
    "        lda #$A0",
    "        jsr $FDED",
    "        dex",
    "        dex",
    "        bne dfloop",
    "        lda #$8D",
    "        jsr $FDED",
    "        rts",
    "failtable ds 256",
    };
    for (const auto& testname : testnames) {
        a->w(testname.c_str());
    }
}

void liveTest(const MMUTest::Test& test, MMU_IIgs *mmu_iigs) {
    int testnum = test.number;
    printf("Running: %d - %s\n", testnum, test.description.c_str());
    
    for (const auto& op : test.operations) {
        std::visit([mmu_iigs](auto&& operation) {
            using T = std::decay_t<decltype(operation)>;
            if constexpr (std::is_same_v<T, MMUTest::WriteOp>) {
                // Handle write
                printf("  Write to %02X/%04X: ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                uint32_t address = operation.location;
                for (auto byte : operation.data) {
                    printf("%02X ", byte);
                    mmu_iigs->write(address, byte);
                    address++;
                }
                printf("\n");
            }
            else if constexpr (std::is_same_v<T, MMUTest::ReadOp>) {
                // Handle read
                printf("  Read from %02X/%04X: ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                
                uint32_t address = operation.location;
                uint8_t value = mmu_iigs->read(address);
                printf("%02X ", value);
                printf("\n");
            }
            else if constexpr (std::is_same_v<T, MMUTest::CopyOp>) {
                // Handle copy
                printf("  Copy from %02X/%04X to %02X/%04X\n",
                       MMUTest::GetBank(operation.source),
                       MMUTest::GetAddress(operation.source),
                       MMUTest::GetBank(operation.destination),
                       MMUTest::GetAddress(operation.destination));
                uint32_t source_address = operation.source;
                uint32_t destination_address = operation.destination;
                uint8_t value = mmu_iigs->read(source_address);
                mmu_iigs->write(destination_address, value);
                printf("%06X -> %02X -> %06X\n", source_address, value, destination_address);
            }
            else if constexpr (std::is_same_v<T, MMUTest::AssertOp>) {
                // Handle assert
                printf("  Assert %02X/%04X == ", 
                       MMUTest::GetBank(operation.location),
                       MMUTest::GetAddress(operation.location));
                uint32_t address = operation.location;
                for (auto byte : operation.expected) {
                    //printf("%02X ", byte);
                    uint8_t val = mmu_iigs->read(address);
                    if (val != byte) {
                        printf("    failed: (expected %06X = %02X, got %02X)\n", 
                            address,
                            byte,
                            val);
                    } else {
                        printf("     good: %06X = %02X\n", address, val);
                    }
                    address++;
                }
            }
        }, op);
    }
}

void printUsage(const char *progname) {
    printf("Usage: %s [-a] [-l] [-p]\n", progname);
    printf("  -a  Emit assembly output (test.asm)\n");
    printf("  -l  Run live test against MMU module\n");
    printf("  -p  Print the tests\n");
    printf("\nIf no flags are specified, all operations are performed.\n");
}

int main(int argc, char *argv[]) 
{
    bool emit_assembly = false;
    bool live_test = false;
    bool print_tests = false;
    bool any_flag_set = false;
    int testNumber = -1;

    int opt;
    while ((opt = getopt(argc, argv, "alph")) != -1) {
        switch (opt) {
            case 'a':
                emit_assembly = true;
                any_flag_set = true;
                break;
            case 'l':
                live_test = true;
                any_flag_set = true;
                break;
            case 'p':
                print_tests = true;
                any_flag_set = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    // Parse optional non-option argument as test number
    if (optind < argc) {
        testNumber = atoi(argv[optind]);
    }

    if (!any_flag_set) {
        printf("Usage: %s [-a] [-l] [-p]\n", argv[0]);
        return 1;
    }

    printf("Starting IIgs MMU test...\n\n");

    // Print the tests
    if (print_tests) {
        for (const auto& test : MMUTest::ALL_TESTS) {
            displayTest(test);
            printf("\n");
        }
    }

    // Generate .asm file to run the tests on hardware/emulator
    if (emit_assembly) {
        AsmFile *a = new AsmFile("test.asm");
        EmitAssemblyPreamble(a);
        for (const auto& test : MMUTest::ALL_TESTS) {
            EmitTestAssembly(test, a);
        }
        EmitAssemblyPostamble(a);
        delete a;
        printf("Assembly output written to test.asm\n");
    }

    // Run the tests against our iigs mmu module
    if (live_test) {
        uint8_t *rom = new uint8_t[128*1024];
        // load ROM file into here.
        FILE *rom_file = fopen("/Users/bazyar/src/GSROM/APPLE2GS.ROM", "rb");
        fread(rom, 1, 128*1024, rom_file);
        fclose(rom_file);
        
        MMU_IIe *mmu_iie = new MMU_IIe(256, 128*1024, rom + 0x1'D000);
        MMU_IIgs *mmu_iigs = new MMU_IIgs(256, 8*1024*1024, 128*1024, rom, mmu_iie);
        mmu_iigs->init_map();
        //mmu_iigs->dump_page_table();
        for (const auto& test : MMUTest::ALL_TESTS) {
            if (testNumber != -1 && test.number != testNumber) continue;
            liveTest(test, mmu_iigs);
        }
        delete mmu_iigs;
        delete mmu_iie;
    }

    return 0;
}
