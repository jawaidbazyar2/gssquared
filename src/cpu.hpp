/*
 *   Copyright (c) 2025 Jawaid Bazyar

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

#include <stdint.h>
//#include <SDL3/SDL.h>
#include <SDL3/SDL.h>

#include "memoryspecs.hpp"
#include "clock.hpp"
#include "util/EventQueue.hpp"
#include "util/EventTimer.hpp"
#include "devices.hpp"
#include "SlotData.hpp"
#include "videosystem.hpp"
#include "debugger/trace.hpp"

//#include "clock.hpp"

#define MAX_CPUS 1

#define BRK_VECTOR 0xFFFE
#define IRQ_VECTOR 0xFFFE
#define NMI_VECTOR 0xFFFA
#define RESET_VECTOR 0xFFFC

struct memory_page {
    uint8_t data[GS2_PAGE_SIZE];
};

enum memory_type {
    MEM_RAM,
    MEM_ROM,
    MEM_IO,
};

struct memory_page_info {
    uint16_t start_address;
    uint16_t end_address;
    uint8_t can_read;
    uint8_t can_write;
    memory_type type;
};

struct memory_map {
    memory_page_info page_info[MEMORY_SIZE / GS2_PAGE_SIZE];
    uint8_t *pages_read[MEMORY_SIZE / GS2_PAGE_SIZE];
    uint8_t *pages_write[MEMORY_SIZE / GS2_PAGE_SIZE];
    //memory_page *pages[MEMORY_SIZE / GS2_PAGE_SIZE];
};

enum processor_type {
    PROCESSOR_6502 = 0,
    PROCESSOR_65C02,
    NUM_PROCESSOR_TYPES
};

enum execution_modes_t {
    EXEC_NORMAL = 0,
    EXEC_STEP_INTO,
    EXEC_STEP_OVER
};

// a couple forward declarations
struct cpu_state;
class Mounts;
struct debug_window_t;

typedef int (*execute_next_fn)(cpu_state *cpu);

struct processor_model {
    const char* name;
    execute_next_fn execute_next;
};

extern processor_model processor_models[NUM_PROCESSOR_TYPES];

namespace cpu_6502 {
    extern int execute_next(cpu_state *cpu);
}
namespace cpu_65c02 {
    extern int execute_next(cpu_state *cpu);
}

typedef enum {
    MODULE_DISPLAY = 0,
    MODULE_SPEAKER,
    MODULE_GAMECONTROLLER,
    MODULE_DISKII,
    MODULE_MEMEXP,
    MODULE_LANGCARD,
    MODULE_THUNDERCLOCK,
    MODULE_PRODOS_CLOCK,
    //MODULE_PD_BLOCK2,
    MODULE_PARALLEL,
    MODULE_VIDEX,
    //MODULE_MB,
    MODULE_ANNUNCIATOR,
    MODULE_NUM_MODULES
} module_id_t;

struct cpu_state {
    uint64_t boot_time; 
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

    uint8_t db;   /* Data Bank register */
    uint16_t sp;  /* Stack Pointer */
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
        uint16_t d;   /* Full 16-bit Y Index Register */
    };
    union {
        struct {
            uint8_t C : 1;  /* Carry Flag */
            uint8_t Z : 1;  /* Zero Flag */
            uint8_t I : 1;  /* Interrupt Disable Flag */
            uint8_t D : 1;  /* Decimal Mode Flag */
            uint8_t B : 1;  /* Break Command Flag */
            uint8_t _unused : 1;  /* Unused bit */
            uint8_t V : 1;  /* Overflow Flag */
            uint8_t N : 1;  /* Negative Flag */
        };
        uint8_t p;  /* Processor Status Register */
    };
    uint8_t halt = 0; /* == 1 is HLT instruction halt; == 2 is user halt */
    uint64_t cycles; /* Number of cycles since reset */

    uint8_t *main_ram_64 = nullptr;
    uint8_t *main_io_4 = nullptr;
    uint8_t *main_rom_D0 = nullptr;

    uint64_t irq_asserted = 0; /** bits 0-7 correspond to slot IRQ lines slots 0-7. */

    memory_map *memory;

    int8_t C8xx_slot;
    void (*C8xx_handlers[8])(cpu_state *cpu, SlotType_t slot) = {nullptr};

    uint64_t last_tick;
    uint64_t next_tick;
    uint64_t clock_slip = 0;
    uint64_t clock_busy = 0;
    uint64_t clock_sleep = 0;
    uint64_t cycle_duration_ns;
    //uint64_t cycle_duration_ticks;
    uint64_t HZ_RATE;
    clock_mode_t clock_mode = CLOCK_FREE_RUN;
    float e_mhz = 0;
    
    execute_next_fn execute_next;

    video_system_t *video_system;

    Mounts *mounts;
    EventQueue *event_queue = nullptr;

    void *module_store[MODULE_NUM_MODULES];
    /*void */ SlotData *slot_store[NUM_SLOTS];

    EventTimer event_timer;

    /* Tracing & Debug */
    bool trace = false;
    system_trace_buffer *trace_buffer;
    system_trace_entry_t trace_entry;
    debug_window_t *debug_window;
    execution_modes_t execution_mode = EXEC_NORMAL;
    uint64_t instructions_left = 0;
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

extern struct cpu_state CPUs[MAX_CPUS];

//void reset_system(cpu_state *cpu);

void cpu_reset(cpu_state *cpu);

void run_cpus(void) ;

void toggle_clock_mode(cpu_state *cpu);

void set_clock_mode(cpu_state *cpu, clock_mode_t mode);

const char* processor_get_name(int processor_type);

void *get_module_state(cpu_state *cpu, module_id_t module_id);
void set_module_state(cpu_state *cpu, module_id_t module_id, void *state);

SlotData *get_slot_state(cpu_state *cpu, SlotType_t slot);
SlotData *get_slot_state_by_id(cpu_state *cpu, device_id id);
void set_slot_state(cpu_state *cpu, SlotType_t slot, SlotData *state);

void init_default_memory_map(cpu_state *cpu);

void set_slot_irq(cpu_state *cpu, uint8_t slot, bool irq);
