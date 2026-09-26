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

#include "ModalContainer.hpp"
#include "util/Gs2Url.hpp"

#include <memory>
#include <string>

class Button_t;
struct SDL_Thread;
struct TextRenderer;

/** Confirm, download, or report failure for one gssquared: pack URL. */
class PackFetchModal_t : public ModalContainer_t {
public:
    enum class Result {
        Pending,
        Ready,
        Dismissed,
    };

    PackFetchModal_t(UIContext *ctx, modal_stack& stack);
    ~PackFetchModal_t() override;

    void present_confirm(const gs2url::PackUrl& url, const std::string& dest);
    void present_error(const std::string& error);

    void update() override;
    void render() override;
    void layout() override;

    Result result() const { return result_; }
    const std::string& cache_path() const { return dest_; }

private:
    struct Job;

    void show_confirm();
    void show_downloading();
    void show_failed(const std::string& error);
    void start_download();
    void place();
    static int thread_main(void *userdata);

    /** Body copy. System Select's shared renderer is the large tile font. */
    std::unique_ptr<TextRenderer> body_text_;
    std::string prompt_;
    std::string detail_;
    gs2url::PackUrl url_;
    std::string dest_;
    Result result_ = Result::Pending;
    Button_t *download_btn_ = nullptr;
    Button_t *cancel_btn_ = nullptr;
    Button_t *dismiss_btn_ = nullptr;
    std::unique_ptr<Job> job_;
    SDL_Thread *thread_ = nullptr;
};
