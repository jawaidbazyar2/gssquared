#include <cstring>

#include "mmus/mmu.hpp"
#include "debugger/disasm.hpp"
#include "debugger/trace_opcodes.hpp"
#include "util/HexDecode.hpp"

//           11111111112222222222
// 012345678901234567890123456789
// xxxx: xx xx xx    LDA  $1234

#define TB_ADDRESS 0
#define TB_OP1 6
#define TB_OP2 9
#define TB_OP3 12

#define TB_OPCODE 18
#define TB_OPERAND 23
#define TB_EOL 40

Disassembler::Disassembler(MMU *mmu) : mmu(mmu) {
    address = 0;
    length = 0;
    line_prepend = 0;
}

Disassembler::~Disassembler() {
}

void Disassembler::setAddress(uint32_t address) {
    this->address = address;
}

uint8_t Disassembler::read_mem(uint32_t address) {
    if (address >= 0xC000 && address < 0xC0FF) {
        return 0xEE; // do not actually trigger I/O when we're trying to disassemble.
    }
    return mmu->read(address);
}

void Disassembler::disassemble_one() {
    char buffer[200];
    char snpbuf[100];
    size_t buffer_size = sizeof(buffer);
    size_t snpbuf_size = sizeof(snpbuf);

    memset(buffer, ' ', buffer_size);

    char *bufptr = buffer + line_prepend;

    uint8_t opcode = read_mem(address);
        
    const disasm_entry *da = &disasm_table[opcode];
    const char *opcode_name = da->opcode;
    const address_mode_entry *am = &address_mode_formats[da->mode];
    uint32_t operand;

    if (address & 0xFF0000) {
        decode_hex_byte(bufptr + TB_ADDRESS - 3, address >> 16);
        bufptr[TB_ADDRESS - 1] = '/';
    }

    decode_hex_word(bufptr + TB_ADDRESS, address);
    bufptr[TB_ADDRESS + 4] = ':';
    decode_hex_byte(bufptr + TB_OP1, read_mem(address));

    operand = 0; uint8_t op2, op3;
    if (am->size == 2) {
        op2 = read_mem(address + 1);
        operand = op2;
        decode_hex_byte(bufptr + TB_OP2, op2);
    } else if (am->size == 3) {
        op2 = read_mem(address + 1);
        op3 = read_mem(address + 2);
        operand = op2 | (op3 << 8);
        decode_hex_byte(bufptr + TB_OP2, op2);
        decode_hex_byte(bufptr + TB_OP3, op3);
    }

    if (opcode_name) memcpy(bufptr + TB_OPCODE, opcode_name, strlen(opcode_name));
    else memcpy(bufptr + TB_OPCODE, "???", 3);

    switch (da->mode) {
        case NONE:
            memcpy(bufptr + TB_OPERAND, "???", 3);
            break;
        case ACC:
        case IMP:
            memcpy(bufptr + TB_OPERAND, am->format, strlen(am->format));
            break;

        case ABS:
        case ABS_X:
        case ABS_Y:
        case IMM:
        case INDIR:
        case ABS_IND_X: // 65c02
        case INDEX_INDIR:
        case INDIR_INDEX:
        case ZP:
        case ZP_X:
        case ZP_Y:
        case ZP_IND: // 65c02
        case IND_LONG:
        case IND_Y_LONG:
        case REL_S:
        case REL_S_Y:
        case ABSL:
        case ABSL_X:
        case ABS_IND_LONG:
        case IMM_S:
            snprintf(snpbuf, snpbuf_size, am->format, operand);
            memcpy(bufptr + TB_OPERAND, snpbuf, strlen(snpbuf));
            break;
        case REL: {
                uint16_t btarget = (address+2) + (int8_t)operand;
                snprintf(snpbuf, snpbuf_size, "$%04X", btarget);
                memcpy(bufptr + TB_OPERAND, snpbuf, strlen(snpbuf));
            }
            break;
        case REL_L: {
                uint16_t btargetl = (address+3) + (int16_t)operand;
                snprintf(snpbuf, snpbuf_size, "$%04X", btargetl);
                memcpy(bufptr + TB_OPERAND, snpbuf, strlen(snpbuf));
            }
            break;
        case MOVE:  
            {
                printf("move operand %04X\n", operand);
                snprintf(snpbuf, snpbuf_size, am->format,
                    operand & 0x00FF, operand >> 8);
                memcpy(bufptr + TB_OPERAND, snpbuf, strlen(snpbuf));
            }
            break;

    }
    address += am->size; // get ready for next instruction
    bufptr[TB_EOL] = '\0';
    output_buffer.push_back(buffer);
}

// returns vector of disassembled instructions
std::vector<std::string> Disassembler::disassemble(int count) {
    //printf("disassembling %d instructions\n", count);
    for (int i = 0; i < count; i++) {
        disassemble_one();
    }
    std::vector<std::string> ret(std::move(output_buffer));
    output_buffer.clear();
    return ret;
}