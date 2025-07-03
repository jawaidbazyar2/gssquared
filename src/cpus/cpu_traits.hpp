#pragma once

#include "cpu.hpp"

// Base interface for all CPU implementations
class BaseCPU {
public:
    virtual ~BaseCPU() = default;
    virtual int execute_next(cpu_state *cpu) = 0;
};

// cpu_traits.hpp
struct CPU6502Traits {
    static constexpr bool has_indirect_bug = true;
    static constexpr bool has_stz = false;
    static constexpr bool has_zp_indirect = false;
    static constexpr bool has_bra = false;
    static constexpr bool has_phx_phy = false;
    static constexpr bool has_inc_dec_acc = false;
    static constexpr bool has_tsb_trb = false;
};

struct CPU65C02Traits {
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_stz = true;
    static constexpr bool has_zp_indirect = true;
    static constexpr bool has_bra = true;
    static constexpr bool has_phx_phy = true;
    static constexpr bool has_inc_dec_acc = true;
    static constexpr bool has_tsb_trb = true;
};
