#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "debugger/trace.hpp"

struct cpu_state;

// Wire / logical constants (Docs/DebugProtocol.md)
constexpr uint8_t BP_KIND_EXEC = 1;
constexpr uint8_t BP_KIND_DATA = 2;
constexpr uint8_t BP_KIND_IO = 3;

constexpr uint8_t BP_ACCESS_NONE = 0;
constexpr uint8_t BP_ACCESS_R = 1;
constexpr uint8_t BP_ACCESS_W = 2;
constexpr uint8_t BP_ACCESS_RW = 3;

constexpr uint8_t BP_FLAG_ENABLED = 1 << 0;
constexpr uint8_t BP_FLAG_TEMPORARY = 1 << 1;
constexpr uint8_t BP_FLAG_DATA_MATCH = 1 << 2;
constexpr uint8_t BP_FLAG_VALID_MASK = BP_FLAG_ENABLED | BP_FLAG_TEMPORARY | BP_FLAG_DATA_MATCH;

constexpr uint32_t BP_MAX_ENTRIES = 256;

constexpr uint32_t STOP_BP_EXEC = 1;
constexpr uint32_t STOP_BP_DATA = 2;
constexpr uint32_t STOP_BP_IO = 3;
constexpr uint32_t STOP_STEP = 4;
constexpr uint32_t STOP_PAUSE = 5;

struct bp_entry_t {
    uint32_t id = 0;
    uint8_t kind = 0;
    uint8_t flags = 0;
    uint8_t access = 0;
    uint8_t pad = 0;
    uint32_t domain = 0;
    uint32_t address = 0;
    uint32_t length = 0;
    uint32_t addr_mask = 0xFFFFFFFF;
    uint32_t data_value = 0;
    uint32_t data_mask = 0xFF;
    uint32_t ignore_count = 0;
    uint32_t hit_count = 0;
};

struct StopHit {
    uint32_t reason = 0;
    uint32_t bp_id = 0;
    uint8_t kind = 0;
    uint8_t access = 0;
    uint32_t pc = 0;
    uint32_t eaddr = 0;
    uint32_t value = 0;
};

class BreakpointTable {
public:
    BreakpointTable() = default;

    bool has_enabled() const { return enabled_count_ > 0; }
    size_t size() const { return entries_.size(); }

    /** Returns new id, or 0 on failure (full / invalid). error_msg may be set. */
    uint32_t add(const bp_entry_t &in, const char **error_msg);

    bool clear_id(uint32_t id);
    void clear_all();
    bool set_enabled(uint32_t id, bool enabled);

    const std::vector<bp_entry_t> &entries() const { return entries_; }
    bp_entry_t *find(uint32_t id);

    std::optional<StopHit> check_pre(cpu_state *cpu);
    std::optional<StopHit> check_post(cpu_state *cpu, const system_trace_entry_t *entry);

    /** Policy A: suppress EXEC at pc until PC leaves or one insn retires. */
    void arm_exec_suppress(uint32_t pc);
    void clear_exec_suppress();
    void on_instruction_retired(uint32_t pc_before);

private:
    bool address_match(uint32_t observed, const bp_entry_t &e) const;
    bool io_match(uint32_t eaddr, const bp_entry_t &e) const;
    bool access_match(uint8_t access_flags, bool is_write) const;
    bool data_match(const bp_entry_t &e, uint8_t observed_byte) const;
    std::optional<StopHit> maybe_hit(bp_entry_t &e, uint32_t reason, uint32_t pc,
                                     uint32_t eaddr, uint8_t access, uint32_t value);

    std::vector<bp_entry_t> entries_;
    uint32_t next_id_ = 1;
    uint32_t enabled_count_ = 0;

    bool suppress_exec_active_ = false;
    uint32_t suppress_exec_pc_ = 0;
};
