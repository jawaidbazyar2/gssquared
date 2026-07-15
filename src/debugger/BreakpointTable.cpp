#include "debugger/BreakpointTable.hpp"

#include "cpu.hpp"

namespace {

bool io_bank_ok(uint8_t bank) {
    return bank == 0x00 || bank == 0x01 || bank == 0xE0 || bank == 0xE1;
}

} // namespace

uint32_t BreakpointTable::add(const bp_entry_t &in, const char **error_msg) {
    if (error_msg) {
        *error_msg = nullptr;
    }
    if (entries_.size() >= BP_MAX_ENTRIES) {
        if (error_msg) {
            *error_msg = "too many breakpoints";
        }
        return 0;
    }
    if (in.kind != BP_KIND_EXEC && in.kind != BP_KIND_DATA && in.kind != BP_KIND_IO) {
        if (error_msg) {
            *error_msg = "bad kind";
        }
        return 0;
    }
    if ((in.flags & ~BP_FLAG_VALID_MASK) != 0) {
        if (error_msg) {
            *error_msg = "bad flags";
        }
        return 0;
    }
    if (in.length == 0) {
        if (error_msg) {
            *error_msg = "length == 0";
        }
        return 0;
    }
    if (in.address + in.length < in.address) {
        if (error_msg) {
            *error_msg = "out of range";
        }
        return 0;
    }
    if (in.kind == BP_KIND_EXEC) {
        if (in.access != BP_ACCESS_NONE) {
            if (error_msg) {
                *error_msg = "bad access";
            }
            return 0;
        }
    } else {
        if (in.access != BP_ACCESS_R && in.access != BP_ACCESS_W && in.access != BP_ACCESS_RW) {
            if (error_msg) {
                *error_msg = "bad access";
            }
            return 0;
        }
    }
    if (in.kind == BP_KIND_IO) {
        uint32_t base = in.address & 0xFFFF;
        if (base < 0xC000 || base + in.length < base || base + in.length > 0xC100) {
            if (error_msg) {
                *error_msg = "out of range";
            }
            return 0;
        }
    }

    bp_entry_t e = in;
    e.id = next_id_++;
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    e.hit_count = 0;
    if (e.flags & BP_FLAG_ENABLED) {
        enabled_count_++;
    }
    entries_.push_back(e);
    return e.id;
}

bool BreakpointTable::clear_id(uint32_t id) {
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->id == id) {
            if (it->flags & BP_FLAG_ENABLED) {
                enabled_count_--;
            }
            entries_.erase(it);
            return true;
        }
    }
    return false;
}

void BreakpointTable::clear_all() {
    entries_.clear();
    enabled_count_ = 0;
}

bool BreakpointTable::set_enabled(uint32_t id, bool enabled) {
    bp_entry_t *e = find(id);
    if (!e) {
        return false;
    }
    bool was = (e->flags & BP_FLAG_ENABLED) != 0;
    if (enabled && !was) {
        e->flags |= BP_FLAG_ENABLED;
        enabled_count_++;
    } else if (!enabled && was) {
        e->flags &= ~BP_FLAG_ENABLED;
        enabled_count_--;
    }
    return true;
}

bp_entry_t *BreakpointTable::find(uint32_t id) {
    for (auto &e : entries_) {
        if (e.id == id) {
            return &e;
        }
    }
    return nullptr;
}

bool BreakpointTable::address_match(uint32_t observed, const bp_entry_t &e) const {
    uint32_t masked_a = observed & e.addr_mask;
    uint32_t masked_base = e.address & e.addr_mask;
    return masked_base <= masked_a && masked_a < masked_base + e.length;
}

bool BreakpointTable::io_match(uint32_t eaddr, const bp_entry_t &e) const {
    uint8_t bank = static_cast<uint8_t>((eaddr >> 16) & 0xFF);
    uint32_t offset = eaddr & 0xFFFF;
    uint32_t base = e.address & 0xFFFF;
    if (!io_bank_ok(bank)) {
        return false;
    }
    return base <= offset && offset < base + e.length;
}

bool BreakpointTable::access_match(uint8_t access_flags, bool is_write) const {
    if (access_flags == BP_ACCESS_RW) {
        return true;
    }
    if (access_flags == BP_ACCESS_W) {
        return is_write;
    }
    if (access_flags == BP_ACCESS_R) {
        return !is_write;
    }
    return false;
}

bool BreakpointTable::data_match(const bp_entry_t &e, uint8_t observed_byte) const {
    if ((e.flags & BP_FLAG_DATA_MATCH) == 0) {
        return true;
    }
    uint8_t mask = static_cast<uint8_t>(e.data_mask & 0xFF);
    uint8_t want = static_cast<uint8_t>(e.data_value & 0xFF);
    return (observed_byte & mask) == (want & mask);
}

std::optional<StopHit> BreakpointTable::maybe_hit(bp_entry_t &e, uint32_t reason, uint32_t pc,
                                                  uint32_t eaddr, uint8_t access, uint32_t value) {
    e.hit_count++;
    if (e.ignore_count > 0) {
        e.ignore_count--;
        return std::nullopt;
    }
    StopHit hit;
    hit.reason = reason;
    hit.bp_id = e.id;
    hit.kind = e.kind;
    hit.access = access;
    hit.pc = pc;
    hit.eaddr = eaddr;
    hit.value = value;
    if (e.flags & BP_FLAG_TEMPORARY) {
        clear_id(e.id);
    }
    return hit;
}

std::optional<StopHit> BreakpointTable::check_pre(cpu_state *cpu) {
    if (!cpu || enabled_count_ == 0) {
        return std::nullopt;
    }
    uint32_t fullpc = cpu->full_pc;
    if (suppress_exec_active_) {
        if (fullpc != suppress_exec_pc_) {
            clear_exec_suppress();
        } else {
            return std::nullopt;
        }
    }
    for (size_t i = 0; i < entries_.size();) {
        bp_entry_t &e = entries_[i];
        if ((e.flags & BP_FLAG_ENABLED) == 0 || e.kind != BP_KIND_EXEC) {
            ++i;
            continue;
        }
        if (!address_match(fullpc, e)) {
            ++i;
            continue;
        }
        uint32_t id = e.id;
        auto hit = maybe_hit(e, STOP_BP_EXEC, fullpc, 0, BP_ACCESS_NONE, 0);
        if (hit) {
            return hit;
        }
        size_t j = 0;
        for (; j < entries_.size(); ++j) {
            if (entries_[j].id == id) {
                i = j + 1;
                break;
            }
        }
        if (j >= entries_.size()) {
            break;
        }
    }
    return std::nullopt;
}

std::optional<StopHit> BreakpointTable::check_post(cpu_state *cpu, const system_trace_entry_t *entry) {
    if (!cpu || !entry || enabled_count_ == 0) {
        return std::nullopt;
    }
    uint32_t fullpc = (static_cast<uint32_t>(entry->pb) << 16) | entry->pc;
    uint32_t eaddr = entry->eaddr;
    bool is_write = entry->f_write != 0;
    uint8_t observed = static_cast<uint8_t>(entry->data & 0xFF);
    uint8_t access = is_write ? BP_ACCESS_W : BP_ACCESS_R;

    // Iterate by index so temporary clear is safe
    for (size_t i = 0; i < entries_.size();) {
        bp_entry_t &e = entries_[i];
        if ((e.flags & BP_FLAG_ENABLED) == 0) {
            ++i;
            continue;
        }
        bool matched = false;
        uint32_t reason = 0;
        if (e.kind == BP_KIND_DATA) {
            if (address_match(eaddr, e) && access_match(e.access, is_write) && data_match(e, observed)) {
                matched = true;
                reason = STOP_BP_DATA;
            }
        } else if (e.kind == BP_KIND_IO) {
            if (io_match(eaddr, e) && access_match(e.access, is_write) && data_match(e, observed)) {
                matched = true;
                reason = STOP_BP_IO;
            }
        }
        if (!matched) {
            ++i;
            continue;
        }
        uint32_t id = e.id;
        auto hit = maybe_hit(e, reason, fullpc, eaddr, access, observed);
        if (hit) {
            return hit;
        }
        // ignored hit; entry may still exist — find next index by id
        size_t j = 0;
        for (; j < entries_.size(); ++j) {
            if (entries_[j].id == id) {
                i = j + 1;
                break;
            }
        }
        if (j >= entries_.size()) {
            break;
        }
    }
    return std::nullopt;
}

void BreakpointTable::arm_exec_suppress(uint32_t pc) {
    suppress_exec_active_ = true;
    suppress_exec_pc_ = pc;
}

void BreakpointTable::clear_exec_suppress() {
    suppress_exec_active_ = false;
    suppress_exec_pc_ = 0;
}

void BreakpointTable::on_instruction_retired(uint32_t pc_before) {
    if (!suppress_exec_active_) {
        return;
    }
    // Clear after one instruction at the suppressed PC retires, or if PC already left.
    (void)pc_before;
    suppress_exec_active_ = false;
    suppress_exec_pc_ = 0;
}
