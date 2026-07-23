#pragma once

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "debugger/BreakpointTable.hpp"
#include "debugger/MemoryWatch.hpp"
#include "debugger/disasm.hpp"
#include "debugger/trace.hpp"
#include "mmus/mmu.hpp"

struct mon_range_t {
    uint32_t lo;
    uint32_t hi;
};

enum mon_node_type_t {
    MON_NODE_TYPE_NUMBER,       // single address
    MON_NODE_TYPE_ADDRESS_SET,  // single address with implicit SET command
    MON_NODE_TYPE_RANGE,        // address range
    MON_NODE_TYPE_COMMAND,      // command word
    MON_NODE_TYPE_STRING,       // string (like filename)
};

enum mon_cmd_type_t {
    MON_CMD_UNKNOWN,
    MON_CMD_MOVE,
    MON_CMD_VERIFY,
    MON_CMD_LOAD,
    MON_CMD_SAVE,
    MON_CMD_SET,
    MON_CMD_WATCH,
    MON_CMD_NOWATCH,
    MON_CMD_HELP,
    MON_CMD_BP,
    MON_CMD_BPD,
    MON_CMD_BPI,
    MON_CMD_NOBP,
    MON_CMD_LIST,
    MON_CMD_MAP,
    MON_CMD_DEBUG,
    MON_CMD_NODEBUG,
    MON_CMD_SLOAD,
    MON_CMD_SCLEAR,
    MON_CMD_SLOOKUP,
};

struct mon_node_entry_t {
    mon_node_type_t type;
    mon_cmd_type_t val_cmd;
    uint32_t val_address;
    mon_range_t val_range;
    std::string val_string;
    uint32_t val_number;
};

class Monitor {
public:
    Monitor() = default;

    void bind(MMU *mmu, MemoryWatch *watches, BreakpointTable *breakpoints, Disassembler *disasm,
              std::vector<std::string> *debug_displays, system_trace_buffer *trace_buffer);

    /** Parse + run one line; returns output valid until the next execute(). */
    const std::vector<std::string> &execute(const std::string &line);

private:
    MMU *mmu_ = nullptr;
    MemoryWatch *watches_ = nullptr;
    BreakpointTable *breakpoints_ = nullptr;
    Disassembler *disasm_ = nullptr;
    std::vector<std::string> *debug_displays_ = nullptr;
    system_trace_buffer *trace_ = nullptr;

    std::vector<mon_node_entry_t> nodes_;
    std::vector<std::string> output_;
    uint8_t last_bank_ = 0; // sticky IIgs monitor bank (BB in BB/AAAA)

    void parse(const std::string &line);
    void dump_nodes() const;
    void dispatch();
    static mon_cmd_type_t lookup_cmd(const std::string &cmd);

    void addOutput(const std::vector<std::string> &lines);
    void addOutput(const std::string &line);

    template <typename... Args>
    void addFormattedOutput(const char *format, Args... args) {
        constexpr size_t buffer_size = 1024;
        char buffer[buffer_size];
        snprintf(buffer, buffer_size, format, args...);
        output_.push_back(std::string(buffer));
    }

    static bool is_hex(const std::string &s);
    /** Format 24-bit address as BB/AAAA. */
    static std::string format_addr(uint32_t addr);
    /** Format range as BB/AAAA.ZZZZ (same bank) or BB/AAAA.CC/ZZZZ if banks differ. */
    static std::string format_range(uint32_t lo, uint32_t hi);
    /** Parse BB/AAAA or bare hex. On BB/, updates last_bank_ and stores full 24-bit.
     *  Bare hex is stored literally (no sticky). Returns false on malformed token. */
    bool parse_addr_token(const std::string &tok, uint32_t *out);
    /** Parse BB/AAAA.ZZZZ or AAAA.ZZZZ. Incomplete forms fold sticky bank into lo/hi. */
    bool parse_range_token(const std::string &tok, mon_range_t *out);
    /** Resolve a bare NUMBER used as an address: ≤0xFFFF gets sticky bank. */
    uint32_t as_address(uint32_t v) const;

    bool parse_bp_addr_len(const mon_node_entry_t &node, uint32_t *address, uint32_t *length) const;
    static bool parse_bp_access(const mon_node_entry_t &node, uint8_t *access);

    void peek_byte();
    void poke_bytes();
    void dump_range();

    void cmd_watch();
    void cmd_nowatch();
    void cmd_bp();
    void cmd_bpd_bpi(uint8_t kind);
    void cmd_nobp();
    void cmd_list();
    void cmd_help();
    void cmd_set();
    void cmd_save();
    void cmd_load();
    void cmd_sload();
    void cmd_slookup();
    void cmd_sclear();
    void cmd_map();
    void cmd_move();
    void cmd_debug();
    void cmd_nodebug();
};
