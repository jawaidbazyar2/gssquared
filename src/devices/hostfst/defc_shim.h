/*
 * Minimal GSPlus defc.h stand-in for Host FST sources.
 * Copyright (c) 2025-2026 Jawaid Bazyar — GSSquared adapter.
 * Host FST logic remains Kelvin Sherlock / GSPlus (GPL 2.0).
 */
#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

typedef uint8_t byte;
typedef uint16_t word16;
typedef uint32_t word32;
typedef uint64_t word64;

#ifndef O_BINARY
#define O_BINARY 0
#endif

typedef struct Engine_reg {
    word32 kpc;
    word32 acc;
    word32 xreg;
    word32 yreg;
    word32 stack;
    word32 dbank;
    word32 direct;
    word32 psr;
    word32 flags;
} Engine_reg;

#ifdef __cplusplus
extern "C" {
#endif

extern Engine_reg engine;

word32 get_memory_c(word32 addr, int cycs);
word32 get_memory16_c(word32 addr, int cycs);
word32 get_memory24_c(word32 addr, int cycs);
word32 get_memory32_c(word32 addr, int cycs);
void set_memory_c(word32 addr, word32 val, int cycs);
void set_memory16_c(word32 addr, word32 val, int cycs);
void set_memory24_c(word32 addr, word32 val, int cycs);
void set_memory32_c(word32 addr, word32 val, int cycs);

void host_fst(void);

/** Re-bind host_root to g_cfg_host_path; close host-side open files/cookies. */
void host_fst_remount(void);

#ifdef __cplusplus
}
#endif
