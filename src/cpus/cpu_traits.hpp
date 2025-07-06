#pragma once

struct cpu_state;

// Base interface for all CPU implementations
class BaseCPU {
public:
    virtual ~BaseCPU() = default;
    virtual int execute_next(cpu_state *cpu) = 0;
};

// cpu_traits.hpp
struct CPU6502Traits {
    static constexpr bool has_indirect_bug = true;
    static constexpr bool has_65c02_ops = false;
    static constexpr bool has_bbr_bbs = false;
};

struct CPU65C02Traits {
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
};

struct CPUR65C02Traits {
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = true;
};

struct CPUWDC65C02Traits {
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = true;
};
