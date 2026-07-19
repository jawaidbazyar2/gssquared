/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */
#pragma once

struct cpu_state;

/** Bind guest memory/register access for Host FST for the duration of a call. */
void hostfst_bind_cpu(cpu_state *cpu);

/** Sync engine registers from cpu_state before host_fst(). */
void hostfst_sync_engine_from_cpu(cpu_state *cpu);

/** Sync cpu_state from engine registers after host_fst(). */
void hostfst_sync_cpu_from_engine(cpu_state *cpu);

/** Update g_cfg_host_path from the resolved Host FST directory. */
void hostfst_set_host_path(const char *path);
