/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <stddef.h>
#include <cstdint>
#include <memory>

#include <SDL3/SDL.h>

#include "SlotData.hpp"
#include "debugger/trace.hpp"
#include "Module_ID.hpp"
#include "cpus/processor_type.hpp"
#include "cpus/cpu_traits.hpp"
#include "device_irq_id.hpp"

#define MAX_CPUS 1

// Emulation mode / 6502 Vectors
#define BRK_VECTOR    0xFFFE
#define IRQ_VECTOR    0xFFFE
#define RESET_VECTOR  0xFFFC
#define NMI_VECTOR    0xFFFA
#define ABORTB_VECTOR 0xFFF8
#define COP_VECTOR    0xFFF4

// Native mode / 65816 Vectors
#define N_IRQ_VECTOR    0xFFEE
#define N_NMI_VECTOR    0xFFEA
#define N_ABORTB_VECTOR 0xFFE8
#define N_BRK_VECTOR    0xFFE6
#define N_COP_VECTOR    0xFFE4

enum execution_modes_t {
    EXEC_NORMAL = 0,
    EXEC_STEP_INTO,
    //EXEC_STEP_OVER // no longer used?
};

// a couple forward declarations
struct cpu_state;
class Mounts;
struct debug_window_t;


struct cpu_state {
    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint16_t pc;  /* Program Counter - lower 16 bits of the 24-bit program counter */
                uint8_t pb;   /* Program Bank - upper 8 bits of the 24-bit program counter */
                uint8_t unused; /* Padding to align with 32-bit full_pc */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t unused; /* Padding to align with 32-bit full_pc */
                uint8_t pb;   /* Program Bank - upper 8 bits of the 24-bit program counter */
                uint16_t pc;  /* Program Counter - lower 16 bits of the 24-bit program counter */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint32_t full_pc; /* Full 24-bit program counter (with 8 unused bits) */
    };

    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint16_t unused_db;  /* Program Counter - lower 16 bits of the 24-bit program counter */
                uint8_t db;   /* Program Bank - upper 8 bits of the 24-bit program counter */
                uint8_t unused_db_2; /* Padding to align with 32-bit full_pc */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t unused_db_2; /* Padding to align with 32-bit full_pc */
                uint8_t pb;   /* Program Bank - upper 8 bits of the 24-bit program counter */
                uint16_t unused_db;  /* Program Counter - lower 16 bits of the 24-bit program counter */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint32_t full_db; /* Full 24-bit program counter (with 8 unused bits) */
    };
    //uint8_t db;   /* Data Bank register */

    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint8_t sp_lo;  /* Lower 8 bits of Stack Pointer */
                uint8_t sp_hi;  /* Upper 8 bits of Stack Pointer */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t sp_hi;  /* Upper 8 bits of Stack Pointer */
                uint8_t sp_lo;  /* Lower 8 bits of Stack Pointer */
            #else   
                #error "Endianness not supported"
            #endif
        };
        uint16_t sp;  /* Full 16-bit Stack Pointer */
    };

    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint8_t a_lo;  /* Lower 8 bits of Accumulator */
                uint8_t a_hi;  /* Upper 8 bits of Accumulator */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t a_hi;  /* Upper 8 bits of Accumulator */
                uint8_t a_lo;  /* Lower 8 bits of Accumulator */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint16_t a;   /* Full 16-bit Accumulator */
    };
    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint8_t x_lo;  /* Lower 8 bits of X Index Register */
                uint8_t x_hi;  /* Upper 8 bits of X Index Register */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t x_hi;  /* Upper 8 bits of X Index Register */
                uint8_t x_lo;  /* Lower 8 bits of X Index Register */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint16_t x;   /* Full 16-bit X Index Register */
    };
    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint8_t y_lo;  /* Lower 8 bits of Y Index Register */
                uint8_t y_hi;  /* Upper 8 bits of Y Index Register */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t y_hi;  /* Upper 8 bits of Y Index Register */
                uint8_t y_lo;  /* Lower 8 bits of Y Index Register */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint16_t y;   /* Full 16-bit Y Index Register */
    };
    union {
        struct {
            #if SDL_BYTEORDER == SDL_LIL_ENDIAN
                uint8_t d_lo;  /* Lower 8 bits of Direct Page Register */
                uint8_t d_hi;  /* Upper 8 bits of Direct Page Register */
            #elif SDL_BYTEORDER == SDL_BIG_ENDIAN
                uint8_t d_hi;  /* Upper 8 bits of Direct Page Register */
                uint8_t d_lo;  /* Lower 8 bits of Y Index Register */
            #else
                #error "Endianness not supported"
            #endif
        };
        uint16_t d;   /* Full 16-bit Direct Page Register */
    };
    union {
        struct { // 6502 / 65c02 flags.
            uint8_t C : 1;  /* Carry Flag */
            uint8_t Z : 1;  /* Zero Flag */
            uint8_t I : 1;  /* Interrupt Disable Flag */
            uint8_t D : 1;  /* Decimal Mode Flag */
            uint8_t B : 1;  /* Break Command Flag */
            uint8_t _unused : 1;  /* Unused bit */
            uint8_t V : 1;  /* Overflow Flag */
            uint8_t N : 1;  /* Negative Flag */
        };
        struct { // 65816 flags
            uint8_t _C : 1;  /* Carry Flag */
            uint8_t _Z : 1;  /* Zero Flag */
            uint8_t _I : 1;  /* Interrupt Disable Flag */
            uint8_t _D : 1;  /* Decimal Mode Flag */
            uint8_t _X : 1;  /* Index Width Flag 1 = 8 bit, 0 = 16 bit */
            uint8_t _M : 1;  /* Accumulator Width Flag 1 = 8 bit, 0 = 16 bit */
            uint8_t _V : 1;  /* Overflow Flag */
            uint8_t _N : 1;  /* Negative Flag */
        };
        uint8_t p;  /* Processor Status Register */
    };
    uint8_t E : 1;  /* Emulation Flag */

    uint8_t halt = 0; /* == 1 is HLT instruction halt; == 2 is user halt */
    //uint64_t cycles; /* Number of cpu cycles since poweron */

    uint64_t irq_asserted = 0; /** bits 0-7 correspond to slot IRQ lines slots 0-7. */
    uint8_t skip_next_irq_check = 0; /* if set, skip the next IRQ check */

    MMU *mmu = nullptr; // cpu only needs to know about base interface with read() and write().
    
    processor_type cpu_type = PROCESSOR_6502;

    // TODO: these are sort of clock related but more about metrics. Should go somewhere else, maybe computer?
    uint64_t clock_slip = 0;
    double e_mhz = 0;
    double fps = 0;
    float idle_percent = 0.0f;

    //execute_next_fn execute_next;
    std::unique_ptr<BaseCPU> cpun; // CPU instance.
    BaseCPU *core = nullptr;

    void *module_store[MODULE_NUM_MODULES];
    SlotData *slot_store[NUM_SLOTS];

    /* Tracing & Debug */
    /* These are CPU controls, leave them here */
    bool trace = false;
    system_trace_buffer *trace_buffer = nullptr;
    system_trace_entry_t trace_entry;
    execution_modes_t execution_mode = EXEC_NORMAL;
    uint64_t instructions_left = 0;

    //void init();
    cpu_state(processor_type cpu_type);
    ~cpu_state();

    void set_processor(processor_type new_cpu_type);
    void reset();
    
    void set_mmu(MMU *mmu) { this->mmu = mmu; }
    uint64_t fast_refresh = 9;

};

#define HLT_INSTRUCTION 1
#define HLT_USER 2

#define FLAG_C        0b00000001 /* 0x01 */
#define FLAG_Z        0b00000010 /* 0x02 */
#define FLAG_I        0b00000100 /* 0x04 */
#define FLAG_D        0b00001000 /* 0x08 */
#define FLAG_B        0b00010000 /* 0x10 */
#define FLAG_UNUSED   0b00100000 /* 0x20 */
#define FLAG_V        0b01000000 /* 0x40 */
#define FLAG_N        0b10000000 /* 0x80 */

extern struct cpu_state *CPUs[MAX_CPUS];

const char* processor_get_name(int processor_type);

void *get_module_state(cpu_state *cpu, module_id_t module_id);
void set_module_state(cpu_state *cpu, module_id_t module_id, void *state);

SlotData *get_slot_state(cpu_state *cpu, SlotType_t slot);
SlotData *get_slot_state_by_id(cpu_state *cpu, device_id id);
void set_slot_state(cpu_state *cpu, SlotType_t slot, SlotData *state);

void set_slot_irq(cpu_state *cpu, uint8_t slot, bool irq);
void set_device_irq(cpu_state *cpu, device_irq_id id, bool irq);
