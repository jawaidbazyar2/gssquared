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

#include "agent/EventQueue.hpp"

#include <utility>

namespace agent {

EventQueue::EventQueue(std::size_t capacity) : capacity_(capacity) {}

bool EventQueue::push(std::vector<std::uint8_t> packet) {
    bool dropped_one = false;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (shutting_down_) {
            // Drop silently — agent is going away. Don't count as overflow.
            return true;
        }
        while (q_.size() >= capacity_) {
            q_.pop_front();
            dropped_one = true;
        }
        q_.emplace_back(std::move(packet));
    }
    cv_.notify_one();
    if (dropped_one) {
        dropped_.fetch_add(1, std::memory_order_relaxed);
    }
    return !dropped_one;
}

bool EventQueue::pop_blocking(std::vector<std::uint8_t>& out) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [this] { return !q_.empty() || shutting_down_; });
    if (q_.empty()) {
        // shutting_down_ is true and queue is drained.
        return false;
    }
    out = std::move(q_.front());
    q_.pop_front();
    return true;
}

void EventQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        shutting_down_ = true;
    }
    cv_.notify_all();
}

std::size_t EventQueue::depth() const {
    std::lock_guard<std::mutex> lock(mu_);
    return q_.size();
}

}  // namespace agent
