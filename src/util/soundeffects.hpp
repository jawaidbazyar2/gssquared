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

#include <SDL3/SDL.h>
#include "cpu.hpp"
#include "computer.hpp"

#define SE_SHUGART_DRIVE 0
#define SE_SHUGART_STOP 1
#define SE_SHUGART_HEAD 2
#define SE_SHUGART_OPEN 3
#define SE_SHUGART_CLOSE 4

bool soundeffects_init(computer_t *computer);
void soundeffects_update(bool diskii_running, int tracknumber);
void soundeffects_play(int index);
void soundeffects_shutdown(SDL_AudioDeviceID audio_device);
