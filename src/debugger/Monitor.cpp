#include "debugger/Monitor.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <sstream>

void Monitor::bind(MMU *mmu, MemoryWatch *watches, BreakpointTable *breakpoints, Disassembler *disasm,
                   std::vector<std::string> *debug_displays, system_trace_buffer *trace_buffer) {
    mmu_ = mmu;
    watches_ = watches;
    breakpoints_ = breakpoints;
    disasm_ = disasm;
    debug_displays_ = debug_displays;
    trace_ = trace_buffer;
}

const std::vector<std::string> &Monitor::execute(const std::string &line) {
    output_.clear();
    nodes_.clear();
    parse(line);
    dump_nodes();
    dispatch();
    return output_;
}

void Monitor::addOutput(const std::vector<std::string> &lines) {
    output_.insert(output_.end(), lines.begin(), lines.end());
}

void Monitor::addOutput(const std::string &line) {
    output_.push_back(line);
}

mon_cmd_type_t Monitor::lookup_cmd(const std::string &cmd) {
    if (cmd == "set") return MON_CMD_SET;
    if (cmd == "load") return MON_CMD_LOAD;
    if (cmd == "save") return MON_CMD_SAVE;
    if (cmd == "move") return MON_CMD_MOVE;
    if (cmd == "verify") return MON_CMD_VERIFY;
    if (cmd == "watch") return MON_CMD_WATCH;
    if (cmd == "nowatch") return MON_CMD_NOWATCH;
    if (cmd == "help") return MON_CMD_HELP;
    if (cmd == "bp") return MON_CMD_BP;
    if (cmd == "bpd") return MON_CMD_BPD;
    if (cmd == "bpi") return MON_CMD_BPI;
    if (cmd == "nobp") return MON_CMD_NOBP;
    if (cmd == "list") return MON_CMD_LIST;
    if (cmd == "l") return MON_CMD_LIST;
    if (cmd == "map") return MON_CMD_MAP;
    if (cmd == "debug") return MON_CMD_DEBUG;
    if (cmd == "nodebug") return MON_CMD_NODEBUG;
    if (cmd == "sload") return MON_CMD_SLOAD;
    if (cmd == "sclear") return MON_CMD_SCLEAR;
    if (cmd == "slookup") return MON_CMD_SLOOKUP;
    return MON_CMD_UNKNOWN;
}

void Monitor::dump_nodes() const {
    for (const auto &node : nodes_) {
        std::cout << "node type:" << node.type << " ";
        switch (node.type) {
            case MON_NODE_TYPE_STRING:
                std::cout << "  string: " << node.val_string << std::endl;
                break;
            case MON_NODE_TYPE_NUMBER:
                std::cout << "  number: " << std::hex << node.val_number << std::dec << std::endl;
                break;
            case MON_NODE_TYPE_ADDRESS_SET:
                std::cout << "  address: " << std::hex << node.val_address << std::dec << " (SET)" << std::endl;
                break;
            case MON_NODE_TYPE_RANGE:
                std::cout << "  range: " << std::hex << node.val_range.lo << " - " << node.val_range.hi << std::dec
                          << std::endl;
                break;
            case MON_NODE_TYPE_COMMAND:
                std::cout << "  command: " << node.val_string << "(" << node.val_cmd << ")" << std::endl;
                break;
        }
    }
}

void Monitor::parse(const std::string &line) {
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;

    while (iss >> token) {
        size_t colon_pos = token.find(':');
        if (colon_pos != std::string::npos && colon_pos < token.length() - 1) {
            std::string addr_part = token.substr(0, colon_pos + 1);
            std::string value_part = token.substr(colon_pos + 1);
            tokens.push_back(addr_part);
            tokens.push_back(value_part);
        } else if (!token.empty() && token.back() == 'l') {
            std::string addr_part = token.substr(0, token.length() - 1);
            tokens.push_back("l");
            tokens.push_back(addr_part);
        } else {
            tokens.push_back(token);
        }
    }

    std::cout << "tokens: ";
    for (const auto &t : tokens) {
        std::cout << t << " ";
    }
    std::cout << std::endl;

    for (const std::string &t : tokens) {
        mon_node_entry_t node = {};

        if (t.empty()) continue;

        if (t.front() == '"' && t.back() == '"') {
            node.type = MON_NODE_TYPE_STRING;
            node.val_string = t.substr(1, t.length() - 2);
        } else if (t.back() == ':') {
            node.type = MON_NODE_TYPE_ADDRESS_SET;
            std::string addr_str = t.substr(0, t.length() - 1);
            node.val_address = static_cast<uint32_t>(std::stoul(addr_str, nullptr, 16));
        } else if (t.find('.') != std::string::npos) {
            size_t dot_pos = t.find('.');
            std::string lo_str = t.substr(0, dot_pos);
            std::string hi_str = t.substr(dot_pos + 1);
            bool lo_valid =
                !lo_str.empty() && std::all_of(lo_str.begin(), lo_str.end(), [](char c) { return std::isxdigit(c); });
            bool hi_valid =
                !hi_str.empty() && std::all_of(hi_str.begin(), hi_str.end(), [](char c) { return std::isxdigit(c); });
            if (lo_valid && hi_valid) {
                node.type = MON_NODE_TYPE_RANGE;
                node.val_range.lo = static_cast<uint32_t>(std::stoul(lo_str, nullptr, 16));
                node.val_range.hi = static_cast<uint32_t>(std::stoul(hi_str, nullptr, 16));
            } else {
                node.type = MON_NODE_TYPE_COMMAND;
                std::string lower_t = t;
                std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
                node.val_string = lower_t;
                node.val_cmd = lookup_cmd(lower_t);
            }
        } else if (std::all_of(t.begin(), t.end(), [](char c) { return std::isxdigit(c); })) {
            node.type = MON_NODE_TYPE_NUMBER;
            node.val_number = static_cast<uint32_t>(std::stoul(t, nullptr, 16));
        } else {
            node.type = MON_NODE_TYPE_COMMAND;
            std::string lower_t = t;
            std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
            node.val_string = lower_t;
            node.val_cmd = lookup_cmd(lower_t);
        }

        nodes_.push_back(node);
    }
}

bool Monitor::parse_bp_addr_len(const mon_node_entry_t &node, uint32_t *address, uint32_t *length) {
    if (node.type == MON_NODE_TYPE_RANGE) {
        *address = node.val_range.lo;
        *length = (node.val_range.hi >= node.val_range.lo) ? (node.val_range.hi - node.val_range.lo + 1) : 1;
        return true;
    }
    if (node.type == MON_NODE_TYPE_NUMBER) {
        *address = node.val_number;
        *length = 1;
        return true;
    }
    return false;
}

bool Monitor::parse_bp_access(const mon_node_entry_t &node, uint8_t *access) {
    if (node.type != MON_NODE_TYPE_COMMAND) {
        return false;
    }
    if (node.val_string == "r") {
        *access = BP_ACCESS_R;
        return true;
    }
    if (node.val_string == "w") {
        *access = BP_ACCESS_W;
        return true;
    }
    if (node.val_string == "rw") {
        *access = BP_ACCESS_RW;
        return true;
    }
    return false;
}

void Monitor::dispatch() {
    if (nodes_.empty()) {
        addOutput("Error: no command provided");
        return;
    }

    const auto &node0 = nodes_[0];
    switch (node0.type) {
        case MON_NODE_TYPE_NUMBER:
            peek_byte();
            return;
        case MON_NODE_TYPE_ADDRESS_SET:
            poke_bytes();
            return;
        case MON_NODE_TYPE_RANGE:
            dump_range();
            return;
        case MON_NODE_TYPE_COMMAND:
            break;
        default:
            addOutput("Error: unknown command");
            return;
    }

    switch (node0.val_cmd) {
        case MON_CMD_WATCH:
            cmd_watch();
            break;
        case MON_CMD_NOWATCH:
            cmd_nowatch();
            break;
        case MON_CMD_BP:
            cmd_bp();
            break;
        case MON_CMD_BPD:
            cmd_bpd_bpi(BP_KIND_DATA);
            break;
        case MON_CMD_BPI:
            cmd_bpd_bpi(BP_KIND_IO);
            break;
        case MON_CMD_NOBP:
            cmd_nobp();
            break;
        case MON_CMD_LIST:
            cmd_list();
            break;
        case MON_CMD_HELP:
            cmd_help();
            break;
        case MON_CMD_SET:
            cmd_set();
            break;
        case MON_CMD_SAVE:
            cmd_save();
            break;
        case MON_CMD_LOAD:
            cmd_load();
            break;
        case MON_CMD_SLOAD:
            cmd_sload();
            break;
        case MON_CMD_SLOOKUP:
            cmd_slookup();
            break;
        case MON_CMD_SCLEAR:
            cmd_sclear();
            break;
        case MON_CMD_MAP:
            cmd_map();
            break;
        case MON_CMD_MOVE:
            cmd_move();
            break;
        case MON_CMD_DEBUG:
            cmd_debug();
            break;
        case MON_CMD_NODEBUG:
            cmd_nodebug();
            break;
        case MON_CMD_VERIFY:
            break;
        case MON_CMD_UNKNOWN:
        default:
            addOutput("Error: unknown command");
            break;
    }
}

void Monitor::peek_byte() {
    uint32_t address = nodes_[0].val_number;
    addFormattedOutput("%06X: %02x", address, mmu_->read(address));
}

void Monitor::poke_bytes() {
    uint32_t address = nodes_[0].val_address;
    for (size_t i = 1; i < nodes_.size(); i++) {
        const auto &node = nodes_[i];
        if (node.type == MON_NODE_TYPE_NUMBER) {
            mmu_->write(address, node.val_number);
            address++;
        } else {
            addFormattedOutput("Error: unexpected value type: %d", node.type);
            return;
        }
    }
}

void Monitor::dump_range() {
    uint32_t address = nodes_[0].val_range.lo;
    while (address <= nodes_[0].val_range.hi) {
        std::ostringstream line;
        line << std::hex << std::uppercase;
        line << std::setfill('0') << std::setw(4) << address << ": ";

        for (int i = 0; i < 16; i++) {
            if (address + i <= nodes_[0].val_range.hi) {
                line << std::setfill('0') << std::setw(2) << (int)mmu_->read(address + i) << " ";
            } else {
                line << "   ";
            }
        }
        line << "  ";

        for (int i = 0; i < 16; i++) {
            if (address + i <= nodes_[0].val_range.hi) {
                uint8_t val = mmu_->read(address + i);
                val &= 0x7F;
                if (val < 32 || val > 126) {
                    line << ".";
                } else {
                    line << (char)val;
                }
            } else {
                line << " ";
            }
        }

        addOutput(line.str());
        address += 16;
    }
    addOutput("");
}

void Monitor::cmd_watch() {
    if (!watches_) {
        return;
    }
    if (nodes_.size() < 2) {
        addOutput("Current memory watches:");
        for (const auto &watch : *watches_) {
            std::ostringstream line;
            line << "[" << watch.id << "] " << std::hex << std::uppercase << watch.start;
            if (watch.end != watch.start) {
                line << "." << watch.end;
            }
            addOutput(line.str());
        }
        return;
    }
    const auto &node1 = nodes_[1];
    uint32_t id = 0;
    if (node1.type == MON_NODE_TYPE_RANGE) {
        id = watches_->add(node1.val_range.lo, node1.val_range.hi);
    } else if (node1.type == MON_NODE_TYPE_NUMBER) {
        id = watches_->add(node1.val_number, node1.val_number);
    } else {
        addOutput("Error: expected range as first argument");
        return;
    }
    addFormattedOutput("Watch id=%u set", id);
}

void Monitor::cmd_nowatch() {
    if (!watches_) {
        return;
    }
    if (nodes_.size() < 2) {
        addOutput("Error: expected watch id");
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type == MON_NODE_TYPE_NUMBER) {
        if (!watches_->remove(node1.val_number)) {
            addOutput("Error: unknown watch id");
        }
    } else {
        addOutput("Error: expected watch id");
    }
}

void Monitor::cmd_bp() {
    if (!breakpoints_) {
        return;
    }
    if (nodes_.size() < 2) {
        addOutput("Current breakpoints:");
        for (const auto &e : breakpoints_->entries()) {
            const char *kind_name = "?";
            if (e.kind == BP_KIND_EXEC) {
                kind_name = "exec";
            } else if (e.kind == BP_KIND_DATA) {
                kind_name = "data";
            } else if (e.kind == BP_KIND_IO) {
                kind_name = "io";
            }
            std::ostringstream line;
            line << "[" << e.id << "] " << kind_name << " " << std::hex << std::uppercase << e.address;
            if (e.length > 1) {
                line << "." << (e.address + e.length - 1);
            }
            if (e.kind == BP_KIND_DATA || e.kind == BP_KIND_IO) {
                if (e.access == BP_ACCESS_R) {
                    line << " r";
                } else if (e.access == BP_ACCESS_W) {
                    line << " w";
                } else if (e.access == BP_ACCESS_RW) {
                    line << " rw";
                }
            }
            addOutput(line.str());
        }
        return;
    }
    uint32_t address = 0;
    uint32_t length = 0;
    if (!parse_bp_addr_len(nodes_[1], &address, &length)) {
        addOutput("Error: expected range as first argument");
        return;
    }
    bp_entry_t e{};
    e.kind = BP_KIND_EXEC;
    e.flags = BP_FLAG_ENABLED;
    e.access = BP_ACCESS_NONE;
    e.domain = 0;
    e.address = address;
    e.length = length;
    e.addr_mask = 0xFFFFFFFF;
    e.data_mask = 0xFF;
    const char *err = nullptr;
    uint32_t id = breakpoints_->add(e, &err);
    if (!id) {
        addFormattedOutput("Error: %s", err ? err : "failed to set breakpoint");
    } else {
        addFormattedOutput("Breakpoint id=%u set", id);
    }
}

void Monitor::cmd_bpd_bpi(uint8_t kind) {
    if (!breakpoints_) {
        return;
    }
    if (nodes_.size() < 3) {
        addOutput("Error: expected address/range and access (r|w|rw)");
        return;
    }
    uint32_t address = 0;
    uint32_t length = 0;
    uint8_t access = BP_ACCESS_NONE;
    if (!parse_bp_addr_len(nodes_[1], &address, &length)) {
        addOutput("Error: expected range as first argument");
        return;
    }
    if (!parse_bp_access(nodes_[2], &access)) {
        addOutput("Error: expected access r|w|rw");
        return;
    }
    bp_entry_t e{};
    e.kind = kind;
    e.flags = BP_FLAG_ENABLED;
    e.access = access;
    e.domain = 0;
    e.address = address;
    e.length = length;
    e.addr_mask = 0xFFFFFFFF;
    e.data_mask = 0xFF;
    const char *err = nullptr;
    uint32_t id = breakpoints_->add(e, &err);
    if (!id) {
        addFormattedOutput("Error: %s", err ? err : "failed to set breakpoint");
    } else {
        addFormattedOutput("Breakpoint id=%u set", id);
    }
}

void Monitor::cmd_nobp() {
    if (!breakpoints_) {
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type == MON_NODE_TYPE_NUMBER) {
        if (!breakpoints_->clear_id(node1.val_number)) {
            bool removed = false;
            for (const auto &e : breakpoints_->entries()) {
                if (e.kind == BP_KIND_EXEC && e.address == node1.val_number) {
                    breakpoints_->clear_id(e.id);
                    removed = true;
                    break;
                }
            }
            if (!removed) {
                addOutput("Error: unknown breakpoint id/address");
            }
        }
    } else {
        addOutput("Error: expected address as first argument");
    }
}

void Monitor::cmd_list() {
    if (!disasm_) {
        return;
    }
    if (nodes_.size() == 2) {
        const auto &node1 = nodes_[1];
        if (node1.type == MON_NODE_TYPE_NUMBER) {
            disasm_->setAddress(node1.val_number);
        }
    }
    addOutput(disasm_->disassemble(30));
}

void Monitor::cmd_help() {
    addOutput("watch range_lo.range_hi      - watch memory range");
    addOutput("watch                        - list watches");
    addOutput("nowatch id                   - remove watch");
    addOutput("bp range_lo.range_hi         - set exec breakpoint");
    addOutput("bpd addr|lo.hi r|w|rw        - data breakpoint");
    addOutput("bpi addr|lo.hi r|w|rw        - I/O breakpoint ($C0xx)");
    addOutput("bp                           - list breakpoints");
    addOutput("nobp address                 - remove breakpoint");
    addOutput("set address value [value...] - set memory values");
    addOutput("address:value [value...]     - set memory values");
    addOutput("list (l) address             - disassemble instructions from address");
    addOutput("list (l)                     - continue disassembly");
    addOutput("load \"filename\" address      - load memory from file");
    addOutput("save \"filename\" lo.hi        - save memory range to file");
    addOutput("move lo.hi address           - move memory from lo to hi to address");
    addOutput("debug \"displayname\"        - add debug display");
    addOutput("nodebug \"displayname\"      - remove debug display");
    addOutput("help                         - this help");
}

void Monitor::cmd_set() {
    const auto &node1 = nodes_[1];
    uint32_t address = node1.val_number;
    for (size_t i = 2; i < nodes_.size(); i++) {
        const auto &node = nodes_[i];
        if (node.type == MON_NODE_TYPE_NUMBER) {
            mmu_->write(address, node.val_number);
            address++;
        } else {
            addFormattedOutput("Error: unexpected value type: %d", node.type);
            return;
        }
    }
}

void Monitor::cmd_save() {
    const auto &node1 = nodes_[1];
    std::string filename;
    if (node1.type == MON_NODE_TYPE_STRING) {
        filename = node1.val_string;
    } else {
        addOutput("Error: expected string 'filename' as first argument");
        return;
    }
    const auto &node2 = nodes_[2];
    if (node2.type == MON_NODE_TYPE_RANGE) {
        FILE *file = fopen(filename.c_str(), "wb");
        if (file) {
            for (uint32_t i = node2.val_range.lo; i <= node2.val_range.hi; i++) {
                uint8_t val = mmu_->read(i);
                fwrite(&val, 1, 1, file);
            }
            fclose(file);
            addFormattedOutput("Saved %d bytes to %s", node2.val_range.hi - node2.val_range.lo + 1, filename.c_str());
        } else {
            addFormattedOutput("Error: could not open file: %s", filename.c_str());
        }
    } else {
        addOutput("Error: expected range as second argument");
    }
}

void Monitor::cmd_load() {
    const auto &node1 = nodes_[1];
    std::string filename;
    if (node1.type == MON_NODE_TYPE_STRING) {
        filename = node1.val_string;
    } else {
        addOutput("Error: expected string 'filename' as first argument");
        return;
    }
    const auto &node2 = nodes_[2];
    if (node2.type == MON_NODE_TYPE_NUMBER) {
        uint32_t address = node2.val_number;
        FILE *file = fopen(filename.c_str(), "rb");
        int i = 0;
        if (file) {
            while (!feof(file)) {
                uint8_t val = 0;
                fread(&val, 1, 1, file);
                mmu_->write(address + i, val);
                i++;
            }
            fclose(file);
            addFormattedOutput("Loaded %d bytes from %s to %04X - %04X", i, filename.c_str(), address, address + i - 1);
        } else {
            addFormattedOutput("Error: could not open file: %s", filename.c_str());
        }
    }
}

void Monitor::cmd_sload() {
    if (trace_ == nullptr) {
        addOutput("Error: trace buffer not set");
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type != MON_NODE_TYPE_STRING) {
        addOutput("Error: expected string 'filename' as first argument");
        return;
    }
    std::string filename = node1.val_string;
    if (!trace_->load_labels_from_file(filename)) {
        addFormattedOutput("Error: could not load symbol table from file: %s", filename.c_str());
        return;
    }
    addFormattedOutput("Loaded symbol table from %s", filename.c_str());
}

void Monitor::cmd_slookup() {
    const auto &node1 = nodes_[1];
    if (node1.type != MON_NODE_TYPE_NUMBER) {
        addOutput("Error: expected number as first argument");
        return;
    }
    addFormattedOutput("%06X: %s", node1.val_number, trace_->get_label(node1.val_number));
}

void Monitor::cmd_sclear() {
    trace_->clear_labels();
    addFormattedOutput("Cleared symbol table");
}

void Monitor::cmd_map() {
    if (nodes_.size() == 1) {
        int pages[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9,
                       0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF, 0xD0, 0xE0};
        addFormattedOutput("Page %10s | %10s", "read", "write");
        for (size_t i = 0; i < sizeof(pages) / sizeof(pages[0]); i++) {
            addFormattedOutput("$%02X: %10s | %10s", pages[i], mmu_->get_read_d(pages[i]), mmu_->get_write_d(pages[i]));
        }
    } else {
        const auto &node1 = nodes_[1];
        addFormattedOutput("Page %10s | %10s", "read", "write");

        int pg_lo = 0, pg_hi = 0;
        if (node1.type == MON_NODE_TYPE_NUMBER) {
            pg_lo = node1.val_number;
            pg_hi = node1.val_number;
        } else if (node1.type == MON_NODE_TYPE_RANGE) {
            pg_lo = node1.val_range.lo;
            pg_hi = node1.val_range.hi;
        } else {
            addOutput("Error: expected number or range as first argument");
            return;
        }
        for (int i = pg_lo; i <= pg_hi; i++) {
            addFormattedOutput("$%02X: %10s | %10s", i, mmu_->get_read_d(i), mmu_->get_write_d(i));
        }
    }
}

void Monitor::cmd_move() {
    if (nodes_.size() < 3) {
        addOutput("Usage: move lo.hi address");
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type != MON_NODE_TYPE_RANGE) {
        addOutput("Error: expected range as first argument");
        return;
    }
    const auto &node2 = nodes_[2];
    if (node2.type != MON_NODE_TYPE_NUMBER) {
        addOutput("Error: expected address as second argument");
        return;
    }
    for (uint32_t i = node1.val_range.lo; i <= node1.val_range.hi; i++) {
        mmu_->write(node2.val_number + (i - node1.val_range.lo), mmu_->read(i));
    }
    addFormattedOutput("Moved %d bytes from %04X to %04X", node1.val_range.hi - node1.val_range.lo + 1,
                       node1.val_range.lo, node2.val_number);
}

void Monitor::cmd_debug() {
    if (!watches_ || !debug_displays_) {
        return;
    }
    if (nodes_.size() == 1) {
        addOutput("Current debug displays:");
        for (auto &display : *debug_displays_) {
            addOutput(display);
        }
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type != MON_NODE_TYPE_STRING) {
        addOutput("Error: expected string as second argument");
        return;
    }
    debug_displays_->push_back(node1.val_string);
    addFormattedOutput("Added debug display: %s", node1.val_string.c_str());
}

void Monitor::cmd_nodebug() {
    if (!watches_ || !debug_displays_) {
        return;
    }
    const auto &node1 = nodes_[1];
    if (node1.type != MON_NODE_TYPE_STRING) {
        addOutput("Error: expected string as second argument");
        return;
    }
    debug_displays_->erase(std::remove(debug_displays_->begin(), debug_displays_->end(), node1.val_string),
                           debug_displays_->end());
}
