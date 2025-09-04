#include <cstring>
#include <fstream>

#include "util/HexDecode.hpp"
#include "debugger/trace.hpp"
#include "debugger/trace_opcodes.hpp"
#include "opcodes.hpp"

    system_trace_buffer::system_trace_buffer(size_t capacity) {
        entries = new system_trace_entry_t[capacity];
        size = capacity;
        head = 0;
        tail = 0;
        count = 0;
    }

    system_trace_buffer::~system_trace_buffer() {
        if (entries != nullptr) {
            delete[] entries;
        }
    }

    void system_trace_buffer::add_entry(const system_trace_entry_t &entry) {
        memcpy(&entries[head], &entry, sizeof(system_trace_entry_t));
        head++;
        if (head >= size) {
            head = 0;
        }
        if (head == tail) {
            tail++;
            count--;
            if (tail >= size) {
                tail = 0;
            }
        }
        count++;
    }   

    void system_trace_buffer::save_to_file(const std::string &filename) {
        printf("Saving trace to file: %s\n", filename.c_str());
        printf("Head: %zu, Tail: %zu, Size: %zu\n", head, tail, size);
        std::ofstream file(filename);
        size_t index = tail;
        while (index != head) {
            // Write the binary record to the file
            file.write(reinterpret_cast<const char*>(&entries[index]), sizeof(system_trace_entry_t));
            index++;
            if (index >= size) {
                index = 0;
            }
        }
        file.close();
    }

    void system_trace_buffer::read_from_file(const std::string &filename) {
        std::ifstream file(filename);
        file.read(reinterpret_cast<char*>(entries), sizeof(system_trace_entry_t) * size);
        file.close();
    }

    system_trace_entry_t *system_trace_buffer::get_entry(size_t index) {
        return &entries[index];
    }


#define TB_A 16
#define TB_X 19
#define TB_Y 22
#define TB_SP 25
#define TB_P 30
#define TB_BAR1 33
#define TB_PC 36
#define TB_OPBYTES 42
#define TB_OPCODE 54
#define TB_OPERAND 59
#define TB_OPERAND2 61
#define TB_OPERAND3 63
#define TB_EADDR 71
#define TB_DATA 76

    char * system_trace_buffer::decode_trace_entry(system_trace_entry_t *entry) {
        static char buffer[256];
        char snpbuf[256];

        size_t buffer_size = sizeof(buffer);
        size_t snpbuf_size = sizeof(snpbuf);
        // if in 8-bit mode
        memset(buffer, ' ', buffer_size);
        snprintf(buffer, buffer_size, "%-12llu ", entry->cycle);
        buffer[13] = ' ' ;
        decode_hex_byte(buffer + TB_A, entry->a);
        decode_hex_byte(buffer + TB_X, entry->x);
        decode_hex_byte(buffer + TB_Y, entry->y);
        memcpy(buffer + TB_SP, "01", 2);
        decode_hex_byte(buffer + TB_SP + 2, entry->sp);
        decode_hex_byte(buffer + TB_P, entry->p);
        buffer[TB_BAR1] = '|';

        const disasm_entry *da = &disasm_table[entry->opcode];
        const char *opcode_name = da->opcode;
        const address_mode_entry *am = &address_mode_formats[da->mode];

        if (entry->flags & TRACE_FLAG_IRQ) {
            memcpy(buffer + TB_OPCODE, "IRQ", 3);
        } else {
            decode_hex_word(buffer + TB_PC, entry->pc);
            buffer[TB_PC + 4] = ':';

            decode_hex_byte(buffer + TB_OPBYTES, entry->opcode);
            if (am->size > 1) {
                decode_hex_byte(buffer + TB_OPBYTES + 3, entry->operand & 0xFF);
            }
            if (am->size > 2) {
                decode_hex_byte(buffer + TB_OPBYTES + 6, (entry->operand >> 8) & 0xFF);
            }

            if (opcode_name) memcpy(buffer + TB_OPCODE, opcode_name, strlen(opcode_name));
            else memcpy(buffer + TB_OPCODE, "???", 3);

            switch (da->mode) {
                case NONE:
                    memcpy(buffer + TB_OPERAND, "???", 3);
                    break;
                case ACC:
                case IMP:
                    memcpy(buffer + TB_OPERAND, am->format, strlen(am->format));
                    break;

                case ABS:
                case ABS_X:
                case ABS_Y:
                case ABS_IND_X: // 65c02
                case IMM:
                case INDIR:
                case INDEX_INDIR:
                case INDIR_INDEX:
                case ZP:
                case ZP_IND: // 65c02
                case ZP_X:
                case ZP_Y:
                    snprintf(snpbuf, snpbuf_size, am->format, entry->operand);
                    memcpy(buffer + TB_OPERAND, snpbuf, strlen(snpbuf));
                    break;
                case REL:
                    uint16_t btarget = (entry->pc+2) + (int8_t)entry->operand;
                    snprintf(snpbuf, snpbuf_size, "$%04X", btarget);
                    memcpy(buffer + TB_OPERAND, snpbuf, strlen(snpbuf));
                    break;
            }
        }

        // print effective memory address
        switch (da->mode) {
            case ACC:
            case IMP:
            case IMM:
            case REL:
                //printf("     ");
                break;
            default:
                decode_hex_word(buffer + TB_EADDR, entry->eaddr);
                break;
        }

        // print memory data
        switch (da->mode) {
            case REL:
            case IMP:
                //printf("   ");
                break;
            
            default:
                if (entry->opcode == OP_JSR_ABS) {
                    decode_hex_word(buffer + TB_DATA, entry->data);
                } else {
                    decode_hex_byte(buffer + TB_DATA, entry->data & 0xFF);
                }
                break;
        }
/*         printf("%02X ", entry->db);
        printf("%02X ", entry->pb);
        printf("%04X ", entry->d); */
        buffer[120] = '\0';
//        printf("\n");
        // else if in 16-bit mode
        return buffer;
//        printf("%s\n", buffer);
#if 0
        printf("%-20llu ", entry->cycle);
        printf("%06X ", entry->pc);
        printf("%04X ", entry->a);
        printf("%04X ", entry->x);
        printf("%04X ", entry->y);
        printf("%04X ", entry->sp);
        printf("%02X ", entry->p);
        printf("%02X ", entry->opcode);
        printf("%06X ", entry->operand);
        printf("%06X ", entry->eaddr);
        printf("%04X ", entry->data);
/*         printf("%02X ", entry->db);
        printf("%02X ", entry->pb);
        printf("%04X ", entry->d); */
        printf("\n");
        // else if in 16-bit mode
#endif
    }
