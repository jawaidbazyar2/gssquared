/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <memory>

struct SDL_Thread;

/**
 * One background GET of the updates TOML. Start from System Select iterate,
 * not from the SelectSystem constructor (auto-launch would stall on join).
 */
class UpdateCheck {
public:
    UpdateCheck();
    ~UpdateCheck();

    UpdateCheck(const UpdateCheck&) = delete;
    UpdateCheck& operator=(const UpdateCheck&) = delete;

    /** Spawn the GET once. Later calls are no-ops. */
    void start();
    /** Join if the worker finished; then available() is stable. */
    void poll();
    bool available() const { return available_; }

    /** Cancel and join. Safe if never started. */
    void cancel();

private:
    struct Job;
    static int thread_main(void *userdata);

    std::unique_ptr<Job> job_;
    SDL_Thread *thread_ = nullptr;
    bool started_ = false;
    bool finished_ = false;
    bool available_ = false;
};
