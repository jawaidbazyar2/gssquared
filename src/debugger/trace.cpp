#include <cstring>
#include <fstream>

#include "util/HexDecode.hpp"
#include "debugger/trace.hpp"
#include "debugger/trace_opcodes.hpp"
#include "opcodes.hpp"
#include "debugger/line_buffer.hpp"

    system_trace_buffer::system_trace_buffer(size_t capacity, processor_type cpu_type) {
        entries = new system_trace_entry_t[capacity];
        size = capacity;
        head = 0;
        tail = 0;
        count = 0;
        cpu_mask = 0;
        set_cpu_type(cpu_type);
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

char *system_trace_buffer::decode_trace_entry(system_trace_entry_t *entry) {
    if (cpu_type == PROCESSOR_6502) return decode_trace_entry_6502(entry);
    if (cpu_type == PROCESSOR_65C02) return decode_trace_entry_6502(entry);
    if (cpu_type == PROCESSOR_65816) return decode_trace_entry_65816(entry);
    return nullptr;
}

// TODO: need to tell the tracer what cpu type it is.
    char * system_trace_buffer::decode_trace_entry_6502(system_trace_entry_t *entry) {
        static line_buffer buffer;
        char snpbuf[256];

        buffer.reset();

        size_t snpbuf_size = sizeof(snpbuf);

        buffer.put(entry->cycle, 12);
        //buffer.pos(TB_A);
        buffer.put(' ');

        buffer.put((uint8_t)entry->a);
        buffer.put(' ');
        buffer.put((uint8_t)entry->x);
        buffer.put(' ');
        buffer.put((uint8_t)entry->y);
        buffer.put(' ');    

        buffer.put((uint8_t) 0x01);
        buffer.put((uint8_t)entry->sp);
        buffer.put(' ');
        buffer.put((uint8_t)entry->p);
        buffer.put(' ');

        const disasm_entry *da = &disasm_table[entry->opcode];
        const char *opcode_name = da->opcode;
        const address_mode_entry *am = &address_mode_formats[da->mode];

        if (entry->f_irq) {
            buffer.pos(TB_OPCODE);
            buffer.put("IRQ");
        } else {
            buffer.pos(TB_PC);
            buffer.put((uint16_t) entry->pc);
            buffer.put(": ");

            buffer.put((uint8_t) entry->opcode);
            buffer.put(' ');

            uint32_t x_op = entry->operand;
            int sz = am->size;
            for (int i = 1; i < 4; i++) {
                if (i < sz) {
                    buffer.put((uint8_t) (x_op & 0xFF));
                    buffer.put(' ');
                } else {
                    buffer.put("   ");
                }
                x_op >>= 8;
            }
            buffer.pos(TB_OPCODE);
            // if the opcode is valid, but not match current cpu, then treat as unknown opcode.
            if ((opcode_name) && (da->cpu_mask & cpu_mask)) {
                buffer.put((char *)opcode_name);
            }
            else {
                buffer.put("???");
            }
            buffer.put("  ");

            buffer.pos(TB_OPERAND);
            switch (da->mode) {
                case NONE:
                    buffer.put("???");
                    break;
                case ACC:
                case IMP:
                    buffer.put((char *)am->format);
                    break;

                case IMM: // manually handle 16-bit immediate mode.
                    if (entry->f_op_sz == 2) {
                        buffer.put("#$");
                        buffer.put((uint16_t) entry->operand);
                    } else {
                        buffer.put("#$");
                        buffer.put((uint8_t) entry->operand);
                    }
                    break;

                case ABS:                case ABS_X:                case ABS_Y:                case ABS_IND_X: // 65c02
                case INDIR:              case INDEX_INDIR:          case INDIR_INDEX:          case ZP:
                case ZP_IND: /* 65c02 */ case ZP_X:                 case ZP_Y:
                case ABSL:               case ABSL_X:
                case IND_LONG:           case IND_Y_LONG: case REL_S: case REL_S_Y:
                case IMM_S:              case MOVE:
                    snprintf(snpbuf, snpbuf_size, am->format, entry->operand);
                    buffer.put(snpbuf);
                    break;
                case REL_L:
                case ABS_IND_LONG:
                    buffer.put("XXX");
                    break;
                case REL:
                    uint16_t btarget = (entry->pc+2) + (int8_t)entry->operand;
                    buffer.put("$");
                    buffer.put((uint16_t) btarget);
                    break;
            }
        }

        // print effective memory address
        switch (da->mode) {
            case ACC:             case IMP:            case IMM:            case REL:
                break;
            default:
                buffer.pos(TB_EADDR);
                buffer.put((uint16_t) entry->eaddr);
                break;
        }

        // print memory data
        buffer.pos(TB_DATA);
        switch (da->mode) {
            case REL:
            case IMP:
                //printf("   ");
                break;
            
            default:
                if (entry->opcode == OP_JSR_ABS) {
                    buffer.put((uint16_t) entry->data);
                } else {
                    buffer.put((uint8_t) (entry->data & 0xFF));
                }
                break;
        }

        return buffer.get();
    }

    char * system_trace_buffer::decode_trace_entry_65816(system_trace_entry_t *entry) {
        static line_buffer buffer;
        char snpbuf[256];

        buffer.reset();

        size_t snpbuf_size = sizeof(snpbuf);

        buffer.put(entry->cycle, 10);
        buffer.put(' ');

        if (entry->p & 0x20) {
            buffer.put("  ");
            buffer.put((uint8_t)entry->a); // 8 bit accumulator
        } else {
            buffer.put((uint16_t)entry->a);
        }
        buffer.put(' ');

        if (entry->p & 0x10) { // 8 bit index
            buffer.put("  ");
            buffer.put((uint8_t)entry->x);
            buffer.put("   ");
            buffer.put((uint8_t)entry->y);
            buffer.put(' ');    
        } else {
            buffer.put((uint16_t)entry->x);
            buffer.put(' ');
            buffer.put((uint16_t)entry->y);
            buffer.put(' ');    
        }

        buffer.put(entry->sp);
        /* buffer.put((uint8_t) 0x01);
        buffer.put((uint8_t)entry->sp); */
        buffer.put(' ');
        buffer.put((uint8_t)entry->p);
        buffer.put(' ');

        const disasm_entry *da = &disasm_table[entry->opcode];
        const char *opcode_name = da->opcode;
        const address_mode_entry *am = &address_mode_formats[da->mode];
        buffer.put(' ');

        if (entry->f_irq) {
            buffer.put("IRQ");
        } else {
            buffer.put(entry->pb);
            buffer.put('/');
            buffer.put(entry->pc);
            buffer.put(": ");

            buffer.put((uint8_t) entry->opcode);
            buffer.put(' ');

            uint32_t x_op = entry->operand;
            int sz = (entry->f_op_sz == 2) ? 3 : am->size;
            for (int i = 1; i < 4; i++) {
                if (i < sz) {
                    buffer.put((uint8_t) (x_op & 0xFF));
                    buffer.put(' ');
                } else {
                    buffer.put("   ");
                }
                x_op >>= 8;
            }

            //buffer.pos(TB_OPCODE+6);
            // if the opcode is valid, but not match current cpu, then treat as unknown opcode.
            if ((opcode_name) && (da->cpu_mask & cpu_mask)) {
                buffer.put((char *)opcode_name);
            }
            else {
                buffer.put("???");
            }
            buffer.put("  ");

            //buffer.pos(TB_OPERAND+6);
            switch (da->mode) {
                case NONE:
                    buffer.put("???");
                    break;
                case ACC:
                case IMP:
                    buffer.put((char *)am->format);
                    break;

                case IMM: // manually handle 16-bit immediate mode.
                    if (entry->f_op_sz == 2) {
                        buffer.put("#$");
                        buffer.put((uint16_t) entry->operand);
                    } else {
                        buffer.put("#$");
                        buffer.put((uint8_t) entry->operand);
                    }
                    break;

                case ABS:                case ABS_X:               case ABS_Y:                case ABS_IND_X: /* 65c02 */
                case INDIR:              case INDEX_INDIR:         case INDIR_INDEX:          case ZP:
                case ZP_IND: /* 65c02 */ case ZP_X:                case ZP_Y:
                case ABSL:               case ABSL_X:
                case IND_LONG:           case IND_Y_LONG: case REL_S: case REL_S_Y:
                case ABS_IND_LONG:
                case IMM_S:              
                    snprintf(snpbuf, snpbuf_size, am->format, entry->operand);
                    buffer.put(snpbuf);
                    break;
                case REL: {
                        uint16_t btarget = (entry->pc+2) + (int8_t)entry->operand;
                        buffer.put("$");
                        buffer.put((uint16_t) btarget);
                    }
                    break;
                case REL_L: {
                        uint16_t btargetl = (entry->pc+3) + (int16_t)entry->operand;
                        buffer.put("$");
                        buffer.put((uint16_t) btargetl);
                    }
                    break;
                case MOVE:  
                    buffer.put("$");
                    buffer.put((uint8_t) (entry->operand & 0x00FF));
                    buffer.put(",$");
                    buffer.put((uint8_t) (entry->operand >> 8));
                    break;
            }
        }

        // print effective memory address
        switch (da->mode) {
            case ACC:            case IMP:            case IMM:            case REL:
                break;
            default:
                buffer.pos(TB_EADDR);
                buffer.put((uint32_t) entry->eaddr);
                break;
        }

        // print memory data
        buffer.pos(TB_EADDR+8);
        switch (da->mode) {
            case REL:
            case IMP:
            case IMM:
                break;
            
            default:
                if (entry->opcode == OP_JSR_ABS || entry->f_data_sz) { // JSR, or, 16-bit data width
                    buffer.put((uint16_t) entry->data);
                } else {
                    buffer.put((uint8_t) (entry->data & 0xFF));
                }
                break;
        }

        return buffer.get();
    }