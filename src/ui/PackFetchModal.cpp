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

#include "PackFetchModal.hpp"

#include "Button.hpp"
#include "platform-specific/HttpsGet.hpp"
#include "util/Gs2Pack.hpp"
#include "util/TextRenderer.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <iostream>

namespace {

Style_t button_style() {
    Style_t style;
    style.background_color = 0xE0E0FFFF;
    style.text_color = 0x000000FF;
    style.border_width = 1;
    style.border_color = 0x000000FF;
    style.padding = 2;
    return style;
}

Style_t panel_style() {
    Style_t style;
    style.background_color = 0xFFFFFFFF;
    style.border_color = 0xFF0000FF;
    style.padding = 3;
    style.border_width = 5;
    style.text_color = 0x000000FF;
    return style;
}

std::string fit_line(TextRenderer *tr, const std::string& text, int max_w) {
    if (text.empty() || tr == nullptr || tr->string_width(text) <= max_w) {
        return text;
    }
    std::string cut = text;
    const std::string ellipsis = "...";
    while (!cut.empty() && tr->string_width(cut + ellipsis) > max_w) {
        cut.pop_back();
        while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80) {
            cut.pop_back();
        }
    }
    return cut + ellipsis;
}

}  // namespace

struct PackFetchModal_t::Job {
    std::string url;
    std::string dest;
    uint64_t max_bytes = 0;
    std::atomic<bool> cancel{false};
    std::atomic<bool> done{false};
    HttpsGetResult result;
};

int PackFetchModal_t::thread_main(void *userdata) {
    auto *job = static_cast<Job *>(userdata);
    https_get_to_file(job->url, job->dest, job->max_bytes, &job->cancel, job->result);
    job->done.store(true);
    return 0;
}

PackFetchModal_t::PackFetchModal_t(UIContext *ctx, modal_stack& stack)
    : ModalContainer_t(ctx, "", panel_style(), stack) {
    if (ctx != nullptr && ctx->renderer != nullptr) {
        body_text_ = std::make_unique<TextRenderer>(ctx->renderer, "fonts/OpenSans-Regular.ttf", 15.0f);
    }
    const Style_t style = button_style();
    download_btn_ = new Button_t(ctx, "Download", style);
    cancel_btn_ = new Button_t(ctx, "Cancel", style);
    dismiss_btn_ = new Button_t(ctx, "Dismiss", style);
    download_btn_->size(140, 32);
    cancel_btn_->size(120, 32);
    dismiss_btn_->size(120, 32);
    download_btn_->on_click([this](const SDL_Event&) -> bool {
        start_download();
        return true;
    });
    cancel_btn_->on_click([this](const SDL_Event&) -> bool {
        if (job_ != nullptr) {
            job_->cancel.store(true);
            return true;
        }
        result_ = Result::Dismissed;
        completed = true;
        return true;
    });
    dismiss_btn_->on_click([this](const SDL_Event&) -> bool {
        result_ = Result::Dismissed;
        completed = true;
        return true;
    });
    add(download_btn_);
    add(cancel_btn_);
    add(dismiss_btn_);
    place();
}

PackFetchModal_t::~PackFetchModal_t() {
    if (job_ != nullptr) {
        job_->cancel.store(true);
    }
    if (thread_ != nullptr) {
        SDL_WaitThread(thread_, nullptr);
        thread_ = nullptr;
    }
}

void PackFetchModal_t::present_confirm(const gs2url::PackUrl& url, const std::string& dest) {
    url_ = url;
    dest_ = dest;
    if (dest_.empty()) {
        present_error("No pack cache directory");
        return;
    }
    show_confirm();
}

void PackFetchModal_t::present_error(const std::string& error) {
    show_failed(error.empty() ? "Download failed" : error);
}

void PackFetchModal_t::show_confirm() {
    prompt_ = "Download from " + url_.host + " and launch?";
    detail_ = url_.filename;
    download_btn_->set_visible(true);
    cancel_btn_->set_visible(true);
    dismiss_btn_->set_visible(false);
    place();
}

void PackFetchModal_t::show_downloading() {
    prompt_ = "Downloading from " + url_.host + "...";
    detail_ = url_.filename;
    download_btn_->set_visible(false);
    cancel_btn_->set_visible(true);
    dismiss_btn_->set_visible(false);
    place();
}

void PackFetchModal_t::show_failed(const std::string& error) {
    prompt_ = error;
    detail_.clear();
    download_btn_->set_visible(false);
    cancel_btn_->set_visible(false);
    dismiss_btn_->set_visible(true);
    place();
}

void PackFetchModal_t::start_download() {
    if (thread_ != nullptr || dest_.empty()) {
        return;
    }
    job_ = std::make_unique<Job>();
    job_->url = url_.https_url;
    job_->dest = dest_;
    job_->max_bytes = gs2pack::kMaxBytes;
    thread_ = SDL_CreateThread(thread_main, "pack-fetch", job_.get());
    if (thread_ == nullptr) {
        job_.reset();
        show_failed("Failed to start download");
        return;
    }
    std::cerr << "Downloading pack " << url_.filename << " from " << url_.host << "\n";
    show_downloading();
}

void PackFetchModal_t::update() {
    Container_t::update();
    if (thread_ == nullptr || job_ == nullptr || !job_->done.load()) {
        return;
    }
    SDL_WaitThread(thread_, nullptr);
    thread_ = nullptr;
    if (job_->result.ok) {
        std::string error;
        if (!gs2url::write_save_token_sidecar(dest_, job_->result.save_token, error) && !error.empty()) {
            std::cerr << "Save token: " << error << "\n";
        }
        result_ = Result::Ready;
        completed = true;
        return;
    }
    if (job_->result.error == "Download canceled") {
        result_ = Result::Dismissed;
        completed = true;
        return;
    }
    show_failed(job_->result.error.empty() ? "Download failed" : job_->result.error);
}

void PackFetchModal_t::render() {
    Container_t::render();
    if (body_text_ == nullptr || (prompt_.empty() && detail_.empty())) {
        return;
    }
    body_text_->set_color((style.text_color >> 24) & 0xFF, (style.text_color >> 16) & 0xFF,
                          (style.text_color >> 8) & 0xFF, style.text_color & 0xFF);
    const int max_w = static_cast<int>(tp.w) - 48;
    const int cx = static_cast<int>(tp.x + tp.w / 2.f);
    const int line_h = body_text_->get_font_line_height();
    int y = static_cast<int>(tp.y + 36.f);
    if (!prompt_.empty()) {
        body_text_->render(fit_line(body_text_.get(), prompt_, max_w), cx, y, TEXT_ALIGN_CENTER);
        y += line_h + 8;
    }
    if (!detail_.empty()) {
        body_text_->render(fit_line(body_text_.get(), detail_, max_w), cx, y, TEXT_ALIGN_CENTER);
    }
}

void PackFetchModal_t::place() {
    int width = 0;
    int height = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    if (ctx != nullptr && ctx->renderer != nullptr) {
        SDL_GetRenderLogicalPresentation(ctx->renderer, &width, &height, &mode);
    }
    if ((width <= 0 || height <= 0) && ctx != nullptr && ctx->window != nullptr) {
        SDL_GetWindowSize(ctx->window, &width, &height);
    }
    if (width <= 0) {
        width = 800;
    }
    if (height <= 0) {
        height = 600;
    }
    const float panel_w = 640.f;
    const float panel_h = 180.f;
    set_position((static_cast<float>(width) - panel_w) / 2.f,
                 (static_cast<float>(height) - panel_h) / 2.f);
    size(panel_w, panel_h);
    layout();
}

void PackFetchModal_t::layout() {
    const float gap = 16.f;
    float total = 0.f;
    int visible = 0;
    for (Tile_t *tile : get_tiles()) {
        if (tile == nullptr || !tile->is_visible()) {
            continue;
        }
        float tile_w = 0.f;
        float tile_h = 0.f;
        tile->get_tile_size(&tile_w, &tile_h);
        total += tile_w;
        ++visible;
    }
    if (visible > 1) {
        total += gap * static_cast<float>(visible - 1);
    }
    float x = tp.x + (tp.w - total) / 2.f;
    const float y = tp.y + tp.h - 56.f;
    for (Tile_t *tile : get_tiles()) {
        if (tile == nullptr || !tile->is_visible()) {
            continue;
        }
        float tile_w = 0.f;
        float tile_h = 0.f;
        tile->get_tile_size(&tile_w, &tile_h);
        tile->set_position(x, y);
        x += tile_w + gap;
    }
}
