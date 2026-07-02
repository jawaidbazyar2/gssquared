/*
 *   Copyright (c) 2026 GSSquared Agent contributors
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

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace agent {

// EventQueue: a bounded SPSC-friendly queue that hands wire-protocol packets
// from the emulator thread (producer) to the IO thread (consumer).
//
// For the initial agent we use mutex+condvar+std::deque. This is fine for VBL
// (60 Hz) and sparse Toolbox events. When we add the high-volume video memory
// write-snoop, we'll swap the implementation for a true lock-free SPSC ring
// buffer behind the same public API.
//
// Overflow policy: drop *oldest* packets. Reason: a back-pressured emulator
// thread is unacceptable; recent state is more relevant than old. The drop
// counter is exposed so the IO thread can emit a "we lost N packets, please
// resync" signal to the compositor when it next has the chance.
class EventQueue {
public:
    explicit EventQueue(std::size_t capacity);

    // Producer side. Called from the emulator thread. Never blocks.
    // Returns true if accepted, false if an older packet had to be dropped
    // to make room (the new packet is still enqueued either way).
    bool push(std::vector<std::uint8_t> packet);

    // Consumer side. Blocks until a packet is available or shutdown() has
    // been called. On shutdown returns false with `out` left unchanged.
    bool pop_blocking(std::vector<std::uint8_t>& out);

    // Wakes any thread blocked in pop_blocking() and causes future calls to
    // return false. Idempotent.
    void shutdown();

    // Stats — safe to read from any thread.
    std::size_t dropped() const { return dropped_.load(std::memory_order_relaxed); }
    std::size_t depth() const;

private:
    const std::size_t capacity_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::vector<std::uint8_t>> q_;
    bool shutting_down_ = false;
    std::atomic<std::size_t> dropped_{0};
};

}  // namespace agent
