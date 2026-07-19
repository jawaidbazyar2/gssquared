/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */
#pragma once

#include "gs2.hpp"

#include <string>

class computer_t;

void init_hostfst(computer_t *computer, SlotType_t slot);

/** Resolved Host FST root (Documents if unset). */
std::string hostfst_resolved_dir();

/** Apply a new host directory (updates settings path + host_root on next startup). */
void hostfst_apply_dir(const std::string &path);
