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

#include "util/UpdateCheck.hpp"

#include "platform-specific/HttpsGet.hpp"
#include "util/CheckForUpdates.hpp"
#include "version.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <string>

namespace {

constexpr uint64_t kMaxBodyBytes = 8192;
constexpr int kTimeoutMs = 8000;

}  // namespace

struct UpdateCheck::Job {
    std::string url;
    std::string user_agent;
    std::atomic<bool> cancel{false};
    std::atomic<bool> done{false};
    HttpsGetResult result;
};

int UpdateCheck::thread_main(void *userdata) {
    auto *job = static_cast<Job *>(userdata);
    https_get_to_string(job->url, kMaxBodyBytes, kTimeoutMs, job->user_agent, &job->cancel,
                        job->result);
    job->done.store(true);
    return 0;
}

UpdateCheck::UpdateCheck() = default;

UpdateCheck::~UpdateCheck() {
    cancel();
}

void UpdateCheck::start() {
    if (started_) {
        return;
    }
    started_ = true;
    job_ = std::make_unique<Job>();
    job_->url = checkForUpdatesTomlUrl();
    job_->user_agent = std::string("GSSquared/") + VERSION_STRING;
    thread_ = SDL_CreateThread(thread_main, "update-check", job_.get());
    if (thread_ == nullptr) {
        finished_ = true;
        job_.reset();
    }
}

void UpdateCheck::poll() {
    if (finished_ || thread_ == nullptr || job_ == nullptr || !job_->done.load()) {
        return;
    }
    SDL_WaitThread(thread_, nullptr);
    thread_ = nullptr;
    finished_ = true;
    if (job_->result.ok) {
        available_ = updateTomlSaysAvailable(job_->result.body);
    }
    job_.reset();
}

void UpdateCheck::cancel() {
    if (job_ != nullptr) {
        job_->cancel.store(true);
    }
    if (thread_ != nullptr) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
    finished_ = true;
    job_.reset();
}
