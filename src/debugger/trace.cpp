#include <cstdint>
#include <cstring>
#include <fstream>
#include <sstream>

#include "util/HexDecode.hpp"
#include "debugger/trace.hpp"
#include "debugger/trace_opcodes.hpp"
#include "opcodes.hpp"
#include "debugger/line_buffer.hpp"

namespace {

constexpr int kLabelWidth = 12;

// 6502/65C02 — SP is one byte; PC is 16-bit.
constexpr trace_column_layout kLayout6502Bytes = {
    /*cycle_w*/ 12,
    /*a*/ 13, /*x*/ 16, /*y*/ 19, /*sp*/ 22, /*p*/ 25,
    /*pc*/ 28, /*opbytes*/ 34, /*opcode*/ 46, /*operand*/ 51,
    /*eaddr*/ 63, /*dir*/ 67, /*data*/ 69, /*label*/ 74, /*label_w*/ kLabelWidth,
};
constexpr trace_column_layout kLayout6502NoBytes = {
    /*cycle_w*/ 12,
    /*a*/ 13, /*x*/ 16, /*y*/ 19, /*sp*/ 22, /*p*/ 25,
    /*pc*/ 28, /*opbytes*/ 34, /*opcode*/ 34, /*operand*/ 39,
    /*eaddr*/ 51, /*dir*/ 55, /*data*/ 57, /*label*/ 62, /*label_w*/ kLabelWidth,
};

// 65816 — wider regs, PB/PC, 24-bit eaddr.
constexpr trace_column_layout kLayout65816Bytes = {
    /*cycle_w*/ 10,
    /*a*/ 11, /*x*/ 16, /*y*/ 21, /*sp*/ 26, /*p*/ 31,
    /*pc*/ 34, /*opbytes*/ 43, /*opcode*/ 55, /*operand*/ 60,
    /*eaddr*/ 72, /*dir*/ 79, /*data*/ 81, /*label*/ 87, /*label_w*/ kLabelWidth,
};
constexpr trace_column_layout kLayout65816NoBytes = {
    /*cycle_w*/ 10,
    /*a*/ 11, /*x*/ 16, /*y*/ 21, /*sp*/ 26, /*p*/ 31,
    /*pc*/ 34, /*opbytes*/ 43, /*opcode*/ 43, /*operand*/ 48,
    /*eaddr*/ 60, /*dir*/ 67, /*data*/ 69, /*label*/ 75, /*label_w*/ kLabelWidth,
};

bool mode_has_eaddr(address_mode mode) {
    switch (mode) {
        case ACC:
        case IMP:
        case IMM:
        case REL:
        case REL_L:
        case NONE:
            return false;
        default:
            return true;
    }
}

bool mode_has_data(address_mode mode) {
    switch (mode) {
        case ACC:
        case IMP:
        case IMM:
        case REL:
        case REL_L:
        case NONE:
            return false;
        default:
            return true;
    }
}

void put_label_truncated(line_buffer &buffer, const char *label, int max_w) {
    if (!label || max_w <= 0) {
        return;
    }
    int n = 0;
    while (label[n] && n < max_w) {
        buffer.put(label[n]);
        n++;
    }
}

void emit_regs_6502(line_buffer &buffer, const system_trace_entry_t *entry,
                    const trace_column_layout &layout) {
    buffer.pos(0);
    buffer.put(entry->cycle, layout.cycle_w);

    buffer.pos(layout.a);
    buffer.put((uint8_t)entry->a);
    buffer.pos(layout.x);
    buffer.put((uint8_t)entry->x);
    buffer.pos(layout.y);
    buffer.put((uint8_t)entry->y);
    buffer.pos(layout.sp);
    buffer.put((uint8_t)entry->sp);
    buffer.pos(layout.p);
    buffer.put((uint8_t)entry->p);
}

void emit_regs_65816(line_buffer &buffer, const system_trace_entry_t *entry,
                     const trace_column_layout &layout) {
    buffer.pos(0);
    buffer.put(entry->cycle, layout.cycle_w);

    buffer.pos(layout.a);
    if (entry->p & 0x20) {
        buffer.put("  ");
        buffer.put((uint8_t)entry->a);
    } else {
        buffer.put((uint16_t)entry->a);
    }

    buffer.pos(layout.x);
    if (entry->p & 0x10) {
        buffer.put("  ");
        buffer.put((uint8_t)entry->x);
    } else {
        buffer.put((uint16_t)entry->x);
    }

    buffer.pos(layout.y);
    if (entry->p & 0x10) {
        buffer.put("  ");
        buffer.put((uint8_t)entry->y);
    } else {
        buffer.put((uint16_t)entry->y);
    }

    buffer.pos(layout.sp);
    buffer.put(entry->sp);
    buffer.pos(layout.p);
    buffer.put((uint8_t)entry->p);
}

void emit_pc(line_buffer &buffer, const system_trace_entry_t *entry,
             const trace_column_layout &layout, bool is_65816) {
    buffer.pos(layout.pc);
    if (is_65816) {
        buffer.put(entry->pb);
        buffer.put('/');
        buffer.put(entry->pc);
    } else {
        buffer.put((uint16_t)entry->pc);
    }
    buffer.put(": ");
}

void emit_opbytes(line_buffer &buffer, const system_trace_entry_t *entry,
                  const trace_column_layout &layout, const address_mode_entry *am,
                  bool show_opbytes) {
    if (!show_opbytes) {
        return;
    }
    buffer.pos(layout.opbytes);
    buffer.put((uint8_t)entry->opcode);
    buffer.put(' ');

    uint32_t x_op = entry->operand;
    int sz = am->size;
    if (entry->f_op_sz == 2) {
        sz = 3; // opcode + 16-bit immediate
    }
    for (int i = 1; i < 4; i++) {
        if (i < sz) {
            buffer.put((uint8_t)(x_op & 0xFF));
            buffer.put(' ');
        } else {
            buffer.put("   ");
        }
        x_op >>= 8;
    }
}

void emit_mnemonic_operand(line_buffer &buffer, const system_trace_entry_t *entry,
                           const trace_column_layout &layout, const disasm_entry *da,
                           const address_mode_entry *am, int16_t cpu_mask,
                           uint32_t *branch_target_out) {
    char snpbuf[256];

    buffer.pos(layout.opcode);
    const char *opcode_name = da->opcode;
    if (opcode_name && (da->cpu_mask & cpu_mask)) {
        buffer.put((char *)opcode_name);
    } else {
        buffer.put("???");
    }

    buffer.pos(layout.operand);
    switch (da->mode) {
        case NONE:
            buffer.put("???");
            break;
        case ACC:
        case IMP:
            buffer.put((char *)am->format);
            break;

        case IMM:
            buffer.put("#$");
            if (entry->f_op_sz == 2) {
                buffer.put((uint16_t)entry->operand);
            } else {
                buffer.put((uint8_t)entry->operand);
            }
            break;

        case ABS:
        case ABS_X:
        case ABS_Y:
        case ABS_IND_X:
        case INDIR:
        case INDEX_INDIR:
        case INDIR_INDEX:
        case ZP:
        case ZP_IND:
        case ZP_X:
        case ZP_Y:
        case ABSL:
        case ABSL_X:
        case IND_LONG:
        case IND_Y_LONG:
        case REL_S:
        case REL_S_Y:
        case ABS_IND_LONG:
        case IMM_S:
            snprintf(snpbuf, sizeof(snpbuf), am->format, entry->operand);
            buffer.put(snpbuf);
            break;

        case REL: {
            uint16_t btarget = (uint16_t)((entry->pc + 2) + (int8_t)entry->operand);
            buffer.put("$");
            buffer.put(btarget);
            if (branch_target_out) {
                *branch_target_out = ((uint32_t)entry->pb << 16) | btarget;
            }
            break;
        }
        case REL_L: {
            uint16_t btargetl = (uint16_t)((entry->pc + 3) + (int16_t)entry->operand);
            buffer.put("$");
            buffer.put(btargetl);
            if (branch_target_out) {
                *branch_target_out = ((uint32_t)entry->pb << 16) | btargetl;
            }
            break;
        }
        case MOVE:
            buffer.put("$");
            buffer.put((uint8_t)(entry->operand & 0x00FF));
            buffer.put(",$");
            buffer.put((uint8_t)(entry->operand >> 8));
            break;
    }
}

void emit_mem(line_buffer &buffer, system_trace_buffer *tb, const system_trace_entry_t *entry,
              const trace_column_layout &layout, const disasm_entry *da, bool is_65816,
              uint32_t branch_target) {
    if (mode_has_eaddr(da->mode)) {
        buffer.pos(layout.eaddr);
        if (is_65816) {
            buffer.put((uint32_t)entry->eaddr);
        } else {
            buffer.put((uint16_t)entry->eaddr);
        }
    }

    if (mode_has_data(da->mode)) {
        buffer.pos(layout.dir);
        buffer.put(entry->f_write ? '<' : '>');
        buffer.pos(layout.data);
        if (entry->opcode == OP_JSR_ABS || entry->f_data_sz) {
            buffer.put((uint16_t)entry->data);
        } else {
            buffer.put((uint8_t)(entry->data & 0xFF));
        }
    }

    // Trailing label: prefer eaddr, else PC, else branch target.
    const char *label = nullptr;
    if (mode_has_eaddr(da->mode)) {
        label = tb->get_label(entry->eaddr);
    }
    if (!label) {
        uint32_t pc_addr = is_65816 ? (((uint32_t)entry->pb << 16) | entry->pc) : entry->pc;
        label = tb->get_label(pc_addr);
    }
    if (!label && branch_target != UINT32_MAX) {
        label = tb->get_label(branch_target);
    }
    if (label) {
        buffer.pos(layout.label);
        put_label_truncated(buffer, label, layout.label_w);
    }
}

void place_header_field(char *buf, size_t buflen, int col, const char *text) {
    if (col < 0 || (size_t)col >= buflen) {
        return;
    }
    size_t n = strlen(text);
    if ((size_t)col + n >= buflen) {
        n = buflen - (size_t)col - 1;
    }
    memcpy(buf + col, text, n);
}

} // namespace

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
        file.write(reinterpret_cast<const char *>(&entries[index]), sizeof(system_trace_entry_t));
        index++;
        if (index >= size) {
            index = 0;
        }
    }
    file.close();
}

void system_trace_buffer::read_from_file(const std::string &filename) {
    std::ifstream file(filename);
    file.read(reinterpret_cast<char *>(entries), sizeof(system_trace_entry_t) * size);
    file.close();
}

system_trace_entry_t *system_trace_buffer::get_entry(size_t index) {
    return &entries[index];
}

bool system_trace_buffer::load_labels_from_file(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        fprintf(stderr, "Warning: Could not open label file: %s\n", filename.c_str());
        return false;
    }

    std::string line;
    int line_count = 0;
    while (std::getline(file, line)) {
        line_count++;

        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        std::istringstream iss(line);
        std::string prefix;
        std::string addr_str;
        std::string label;

        if (!(iss >> prefix >> addr_str >> label)) {
            continue;
        }

        if (prefix != "al") {
            continue;
        }

        uint32_t address = 0;
        try {
            address = std::stoul(addr_str, nullptr, 16);
        } catch (...) {
            fprintf(stderr, "Warning: Invalid address '%s' on line %d\n", addr_str.c_str(), line_count);
            continue;
        }

        if (!label.empty() && label[0] == '.') {
            label = label.substr(1);
        }
        size_t pos = label.find("_0");
        if (pos != std::string::npos) {
            label = label.substr(0, pos);
        }

        labels[address] = label;
        if ((address >= 0xFF'C000) && (address < 0xFF'FFFF)) {
            labels[address & 0xFFFF] = label;
        }
    }

    file.close();
    printf("Loaded %zu labels from %s\n", labels.size(), filename.c_str());
    return true;
}

const char *system_trace_buffer::get_label(uint32_t address) {
    auto it = labels.find(address);
    if (it != labels.end()) {
        return it->second.c_str();
    }
    return nullptr;
}

void system_trace_buffer::clear_labels() {
    labels.clear();
}

const trace_column_layout &system_trace_buffer::get_layout() const {
    const bool bytes = decode_opts.show_opbytes;
    if (cpu_type == PROCESSOR_65816) {
        return bytes ? kLayout65816Bytes : kLayout65816NoBytes;
    }
    return bytes ? kLayout6502Bytes : kLayout6502NoBytes;
}

void system_trace_buffer::format_column_header(char *buf, size_t buflen) const {
    if (!buf || buflen == 0) {
        return;
    }
    memset(buf, ' ', buflen);
    buf[buflen - 1] = '\0';

    const trace_column_layout &layout = get_layout();
    const bool is_65816 = (cpu_type == PROCESSOR_65816);

    // Right-align-ish cycle label in the cycle field.
    place_header_field(buf, buflen, 3, "Cycle");
    place_header_field(buf, buflen, layout.a, "A");
    place_header_field(buf, buflen, layout.x, "X");
    place_header_field(buf, buflen, layout.y, "Y");
    place_header_field(buf, buflen, layout.sp, "SP");
    place_header_field(buf, buflen, layout.p, "P");
    place_header_field(buf, buflen, layout.pc, is_65816 ? "PB/PC" : "PC");
    if (decode_opts.show_opbytes) {
        place_header_field(buf, buflen, layout.opbytes, "Bytes");
    }
    place_header_field(buf, buflen, layout.opcode, "Op");
    place_header_field(buf, buflen, layout.operand, "Operand");
    place_header_field(buf, buflen, layout.eaddr, "Eff");
    place_header_field(buf, buflen, layout.dir, ">");
    place_header_field(buf, buflen, layout.data, "M");
    place_header_field(buf, buflen, layout.label, "Label");

    // Trim trailing spaces for a tidy C string length, keep content.
    size_t end = buflen - 1;
    while (end > 0 && buf[end - 1] == ' ') {
        end--;
    }
    buf[end] = '\0';
}

char *system_trace_buffer::decode_trace_entry(system_trace_entry_t *entry) {
    static line_buffer buffer;

    if (cpu_type != PROCESSOR_6502 && cpu_type != PROCESSOR_65C02 && cpu_type != PROCESSOR_65816) {
        return nullptr;
    }

    buffer.reset();
    const bool is_65816 = (cpu_type == PROCESSOR_65816);
    const trace_column_layout &layout = get_layout();
    const disasm_entry *da = &disasm_table[entry->opcode];
    const address_mode_entry *am = &address_mode_formats[da->mode];

    if (is_65816) {
        emit_regs_65816(buffer, entry, layout);
    } else {
        emit_regs_6502(buffer, entry, layout);
    }

    if (entry->f_irq) {
        buffer.pos(layout.opcode);
        buffer.put("IRQ");
        return buffer.get();
    }

    emit_pc(buffer, entry, layout, is_65816);
    emit_opbytes(buffer, entry, layout, am, decode_opts.show_opbytes);

    uint32_t branch_target = UINT32_MAX;
    emit_mnemonic_operand(buffer, entry, layout, da, am, cpu_mask, &branch_target);
    emit_mem(buffer, this, entry, layout, da, is_65816, branch_target);

    return buffer.get();
}
