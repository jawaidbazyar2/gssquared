#pragma once

struct cpu_state;

// Base interface for all CPU implementations
class BaseCPU {
private:
    const char *name = nullptr;

public:
    virtual ~BaseCPU() = default;
    virtual int execute_next(cpu_state *cpu) = 0;
    virtual void reset(cpu_state *cpu) = 0;
    virtual const char *get_name() = 0;
};

// cpu_traits.hpp
struct CPU6502Traits {
    static constexpr const char *name = "6502";
    static constexpr bool has_indirect_bug = true;
    static constexpr bool has_65c02_ops = false;
    static constexpr bool has_bbr_bbs = false;
    
    static constexpr bool has_65816_ops = false;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65C02Traits {
    static constexpr const char *name = "65C02";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
    
    static constexpr bool has_65816_ops = false;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPUR65C02Traits {
    static constexpr const char *name = "R65C02";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = true;
    
    static constexpr bool has_65816_ops = false;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false; 
    static constexpr bool full_phantom_reads = false;
};

struct CPUWDC65C02Traits {
    static constexpr const char *name = "WDC65C02";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = true;
    
    static constexpr bool has_65816_ops = false;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65816_E_8_8_Traits {
    static constexpr const char *name = "65816 Emulation";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
   
    static constexpr bool has_65816_ops = true;
    static constexpr bool e_mode = true;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65816_N_8_8_Traits {
    static constexpr const char *name = "65816 Native M8 X8";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
   
    static constexpr bool has_65816_ops = true;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65816_N_8_16_Traits {
    static constexpr const char *name = "65816 Native M8 X16";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
   
    static constexpr bool has_65816_ops = true;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = false;
    static constexpr bool x_16 = true;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65816_N_16_8_Traits {
    static constexpr const char *name = "65816 Native M16 X8";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
   
    static constexpr bool has_65816_ops = true;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = true;
    static constexpr bool x_16 = false;
    static constexpr bool full_phantom_reads = false;
};

struct CPU65816_N_16_16_Traits {
    static constexpr const char *name = "65816 Native M16 X16";
    static constexpr bool has_indirect_bug = false;
    static constexpr bool has_65c02_ops = true;
    static constexpr bool has_bbr_bbs = false;
   
    static constexpr bool has_65816_ops = true;
    static constexpr bool e_mode = false;
    static constexpr bool m_16 = true;
    static constexpr bool x_16 = true;
    static constexpr bool full_phantom_reads = false;
};

// Traits for enabling/disabling trace

struct TraceEnabled {
    static constexpr bool trace_enabled = true;
};

struct TraceDisabled {
    static constexpr bool trace_enabled = false;
};