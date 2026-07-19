/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "devices/hostfst/hostfst_engine.hpp"

#if defined(_WIN32) || defined(__EMSCRIPTEN__)

void hostfst_bind_cpu(cpu_state *) {}
void hostfst_sync_engine_from_cpu(cpu_state *) {}
void hostfst_sync_cpu_from_engine(cpu_state *) {}
void hostfst_set_host_path(const char *) {}

#else

#include "devices/hostfst/defc_shim.h"
#include "devices/hostfst/host_common.h"

#include "cpu.hpp"
#include "mmus/mmu.hpp"

#include <cstdlib>
#include <cstring>

Engine_reg engine = {};

static cpu_state *g_hostfst_cpu = nullptr;
static char *g_host_path_owned = nullptr;

void hostfst_bind_cpu(cpu_state *cpu) {
    g_hostfst_cpu = cpu;
}

void hostfst_sync_engine_from_cpu(cpu_state *cpu) {
    engine.acc = cpu->a;
    engine.xreg = cpu->x;
    engine.yreg = cpu->y;
    engine.direct = cpu->d;
    engine.psr = cpu->p;
    engine.stack = cpu->sp;
    engine.dbank = cpu->db;
    engine.kpc = cpu->full_pc & 0xFFFFFF;
}

void hostfst_sync_cpu_from_engine(cpu_state *cpu) {
    cpu->a = static_cast<uint16_t>(engine.acc);
    cpu->x = static_cast<uint16_t>(engine.xreg);
    cpu->y = static_cast<uint16_t>(engine.yreg);
    cpu->d = static_cast<uint16_t>(engine.direct);
    cpu->p = static_cast<uint8_t>(engine.psr);
    cpu->sp = static_cast<uint16_t>(engine.stack);
}

void hostfst_set_host_path(const char *path) {
    free(g_host_path_owned);
    g_host_path_owned = nullptr;
    if (path == nullptr || path[0] == '\0') {
        g_cfg_host_path = const_cast<char *>("");
        return;
    }
    g_host_path_owned = strdup(path);
    g_cfg_host_path = g_host_path_owned ? g_host_path_owned : const_cast<char *>("");
}

static inline MMU *mmu() {
    return g_hostfst_cpu ? g_hostfst_cpu->mmu : nullptr;
}

extern "C" word32 get_memory_c(word32 addr, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return 0;
    }
    return m->read(addr & 0xFFFFFF);
}

extern "C" word32 get_memory16_c(word32 addr, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return 0;
    }
    addr &= 0xFFFFFF;
    return static_cast<word32>(m->read(addr)) |
           (static_cast<word32>(m->read((addr + 1) & 0xFFFFFF)) << 8);
}

extern "C" word32 get_memory24_c(word32 addr, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return 0;
    }
    addr &= 0xFFFFFF;
    return static_cast<word32>(m->read(addr)) |
           (static_cast<word32>(m->read((addr + 1) & 0xFFFFFF)) << 8) |
           (static_cast<word32>(m->read((addr + 2) & 0xFFFFFF)) << 16);
}

extern "C" word32 get_memory32_c(word32 addr, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return 0;
    }
    addr &= 0xFFFFFF;
    return static_cast<word32>(m->read(addr)) |
           (static_cast<word32>(m->read((addr + 1) & 0xFFFFFF)) << 8) |
           (static_cast<word32>(m->read((addr + 2) & 0xFFFFFF)) << 16) |
           (static_cast<word32>(m->read((addr + 3) & 0xFFFFFF)) << 24);
}

extern "C" void set_memory_c(word32 addr, word32 val, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return;
    }
    m->write(addr & 0xFFFFFF, static_cast<uint8_t>(val));
}

extern "C" void set_memory16_c(word32 addr, word32 val, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return;
    }
    addr &= 0xFFFFFF;
    m->write(addr, static_cast<uint8_t>(val));
    m->write((addr + 1) & 0xFFFFFF, static_cast<uint8_t>(val >> 8));
}

extern "C" void set_memory24_c(word32 addr, word32 val, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return;
    }
    addr &= 0xFFFFFF;
    m->write(addr, static_cast<uint8_t>(val));
    m->write((addr + 1) & 0xFFFFFF, static_cast<uint8_t>(val >> 8));
    m->write((addr + 2) & 0xFFFFFF, static_cast<uint8_t>(val >> 16));
}

extern "C" void set_memory32_c(word32 addr, word32 val, int /*cycs*/) {
    MMU *m = mmu();
    if (!m) {
        return;
    }
    addr &= 0xFFFFFF;
    m->write(addr, static_cast<uint8_t>(val));
    m->write((addr + 1) & 0xFFFFFF, static_cast<uint8_t>(val >> 8));
    m->write((addr + 2) & 0xFFFFFF, static_cast<uint8_t>(val >> 16));
    m->write((addr + 3) & 0xFFFFFF, static_cast<uint8_t>(val >> 24));
}

#endif
