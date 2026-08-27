#include "ss_gpu.hpp"

#include <cstdio>
#include <cstring>

void SsGpu::init(video_system_t *video) {
    vs = video;
    slots.assign(MAX_TEXTURES + 1, Slot{});
    csb.assign(MAX_CSB, 0);
}

void SsGpu::shutdown() {
    leave_mode();
    vs = nullptr;
}

void SsGpu::destroy_slot(Slot &s) {
    if (s.tex) {
        SDL_DestroyTexture(s.tex);
        s.tex = nullptr;
    }
    heap_used -= s.bytes;
    s = Slot{};
}

void SsGpu::destroy_display() {
    for (int i = 0; i < 2; i++) {
        if (display[i]) {
            SDL_DestroyTexture(display[i]);
            display[i] = nullptr;
        }
    }
    const uint32_t disp_bytes = (uint32_t)width * (uint32_t)height * 4u * 2u;
    if (heap_used >= disp_bytes) {
        heap_used -= disp_bytes;
    }
}

void SsGpu::leave_mode() {
    for (size_t i = 1; i < slots.size(); i++) {
        destroy_slot(slots[i]);
    }
    destroy_display();
    width = 0;
    height = 0;
    native_format = 0;
    active = false;
    vbl_pending = false;
    heap_used = 0;
    front = 0;
    back = 1;
}

bool SsGpu::enter_mode(uint16_t w, uint16_t h, uint8_t fmt) {
    leave_mode();
    if (!vs || !vs->renderer || w == 0 || h == 0) {
        return false;
    }
    for (int i = 0; i < 2; i++) {
        display[i] = SDL_CreateTexture(vs->renderer, PIXEL_FORMAT,
            SDL_TEXTUREACCESS_TARGET, (int)w, (int)h);
        if (!display[i]) {
            printf("SsGpu: failed to create display target %d (%s)\n", i, SDL_GetError());
            destroy_display();
            return false;
        }
        SDL_SetTextureBlendMode(display[i], SDL_BLENDMODE_NONE);
        SDL_SetTextureScaleMode(display[i], SDL_SCALEMODE_NEAREST);
    }
    width = w;
    height = h;
    native_format = fmt;
    present_policy = 0; // Swap
    active = true;
    front = 0;
    back = 1;
    heap_used = (uint32_t)w * (uint32_t)h * 4u * 2u;
    SDL_SetRenderTarget(vs->renderer, display[front]);
    SDL_SetRenderDrawColor(vs->renderer, 0, 0, 0, 255);
    SDL_RenderClear(vs->renderer);
    SDL_SetRenderTarget(vs->renderer, display[back]);
    SDL_RenderClear(vs->renderer);
    SDL_SetRenderTarget(vs->renderer, nullptr);
    return true;
}

uint32_t SsGpu::bpp_of(uint8_t format) {
    switch (format) {
        case SS_GPU_FMT_IDX8: return 1;
        case SS_GPU_FMT_RGB555: return 2;
        case SS_GPU_FMT_RGB888: return 3;
        case SS_GPU_FMT_ARGB8888: return 4;
        default: return 0;
    }
}

bool SsGpu::convert_to_rgba(uint8_t format, uint16_t w, uint16_t h,
    const uint8_t *src, uint32_t nbytes, std::vector<uint8_t> &out) {
    const uint32_t bpp = bpp_of(format);
    if (bpp == 0) {
        return false;
    }
    const uint32_t expect = (uint32_t)w * (uint32_t)h * bpp;
    if (nbytes < expect) {
        return false;
    }
    out.resize((size_t)w * (size_t)h * 4u);
    uint8_t *dst = out.data();
    const uint32_t npx = (uint32_t)w * (uint32_t)h;
    for (uint32_t i = 0; i < npx; i++) {
        uint8_t r = 0, g = 0, b = 0, a = 255;
        if (format == SS_GPU_FMT_IDX8) {
            r = g = b = src[i];
        } else if (format == SS_GPU_FMT_RGB555) {
            const uint16_t p = (uint16_t)src[i * 2] | ((uint16_t)src[i * 2 + 1] << 8);
            r = (uint8_t)(((p >> 10) & 0x1F) * 255 / 31);
            g = (uint8_t)(((p >> 5) & 0x1F) * 255 / 31);
            b = (uint8_t)((p & 0x1F) * 255 / 31);
        } else if (format == SS_GPU_FMT_RGB888) {
            r = src[i * 3];
            g = src[i * 3 + 1];
            b = src[i * 3 + 2];
        } else {
            /* guest ARGB8888 memory: B,G,R,A */
            b = src[i * 4];
            g = src[i * 4 + 1];
            r = src[i * 4 + 2];
            a = src[i * 4 + 3];
        }
        const RGBA_t px = RGBA_t::make(r, g, b, a);
        memcpy(dst + i * 4, &px, 4);
    }
    return true;
}

SDL_Texture *SsGpu::make_rgba_texture(uint16_t w, uint16_t h, const uint8_t *rgba) {
    SDL_Texture *tex = SDL_CreateTexture(vs->renderer, PIXEL_FORMAT,
        SDL_TEXTUREACCESS_STATIC, (int)w, (int)h);
    if (!tex) {
        return nullptr;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_NONE);
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
    if (!SDL_UpdateTexture(tex, nullptr, rgba, (int)w * 4)) {
        printf("SsGpu: SDL_UpdateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(tex);
        return nullptr;
    }
    return tex;
}

uint16_t SsGpu::alloc_handle() {
    for (uint16_t i = 1; i <= MAX_TEXTURES; i++) {
        if (slots[i].tex == nullptr) {
            return i;
        }
    }
    return INVALID_HANDLE;
}

SsGpu::Slot *SsGpu::slot_of(uint16_t handle) {
    if (handle == 0 || handle == INVALID_HANDLE || handle > MAX_TEXTURES) {
        return nullptr;
    }
    if (slots[handle].tex == nullptr) {
        return nullptr;
    }
    return &slots[handle];
}

uint16_t SsGpu::upload_texture(uint16_t w, uint16_t h, uint8_t format, uint8_t flags,
    const uint8_t *pixels, uint32_t nbytes) {
    (void)flags;
    if (!active || !vs || w == 0 || h == 0 || pixels == nullptr) {
        return INVALID_HANDLE;
    }
    std::vector<uint8_t> rgba;
    if (!convert_to_rgba(format, w, h, pixels, nbytes, rgba)) {
        return INVALID_HANDLE;
    }
    const uint16_t handle = alloc_handle();
    if (handle == INVALID_HANDLE) {
        return INVALID_HANDLE;
    }
    SDL_Texture *tex = make_rgba_texture(w, h, rgba.data());
    if (!tex) {
        return INVALID_HANDLE;
    }
    Slot &s = slots[handle];
    s.tex = tex;
    s.w = w;
    s.h = h;
    s.format = format;
    s.flags = flags;
    s.bytes = (uint32_t)w * (uint32_t)h * 4u;
    heap_used += s.bytes;
    return handle;
}

bool SsGpu::free_texture(uint16_t handle) {
    Slot *s = slot_of(handle);
    if (!s) {
        return false;
    }
    destroy_slot(*s);
    return true;
}

uint8_t *SsGpu::ensure_upload_buf(uint32_t nbytes) {
    if (upload.size() < nbytes) {
        upload.resize(nbytes);
    }
    return upload.data();
}

int SsGpu::handle_count() const {
    int n = 0;
    for (uint16_t i = 1; i <= MAX_TEXTURES; i++) {
        if (slots[i].tex) {
            n++;
        }
    }
    return n;
}

void SsGpu::fill_info(ss_gpu_info_t *out) const {
    out->isa_version = 1;
    out->heap_size = HEAP_SIZE;
    out->heap_free = HEAP_SIZE > heap_used ? HEAP_SIZE - heap_used : 0;
    out->max_textures = MAX_TEXTURES;
    out->max_csb = (uint16_t)MAX_CSB;
    if (active) {
        out->width = width;
        out->height = height;
        out->native_format = native_format;
        out->present_policy = present_policy;
        out->active = 1;
    } else {
        out->width = 0;
        out->height = 0;
        out->native_format = 0;
        out->present_policy = 0;
        out->active = 0;
    }
}

bool SsGpu::exec_csb(const uint8_t *buf, uint32_t len, bool *wait_vbl) {
    *wait_vbl = false;
    if (!active || !vs || buf == nullptr) {
        return false;
    }

    SDL_Texture *old_target = SDL_GetRenderTarget(vs->renderer);
    SDL_Rect clip_i = {};
    const bool clip_on = SDL_RenderClipEnabled(vs->renderer);
    if (clip_on) {
        SDL_GetRenderClipRect(vs->renderer, &clip_i);
    }
    Uint8 cr = 0, cg = 0, cb = 0, ca = 0;
    SDL_GetRenderDrawColor(vs->renderer, &cr, &cg, &cb, &ca);

    SDL_SetRenderTarget(vs->renderer, display[back]);
    SDL_SetRenderClipRect(vs->renderer, nullptr);

    uint32_t pc = 0;
    bool ok = true;
    while (ok && pc < len) {
        const uint8_t op = buf[pc];
        last_op = op;
        if (op == 0x00 || op == 0xFF) {
            break;
        }
        if (op == 0x01) { // Clear color:u32 LE AARRGGBB
            if (pc + 5 > len) {
                ok = false;
                break;
            }
            const uint32_t c = (uint32_t)buf[pc + 1] | ((uint32_t)buf[pc + 2] << 8)
                | ((uint32_t)buf[pc + 3] << 16) | ((uint32_t)buf[pc + 4] << 24);
            const uint8_t a = (uint8_t)(c >> 24);
            const uint8_t r = (uint8_t)(c >> 16);
            const uint8_t g = (uint8_t)(c >> 8);
            const uint8_t b = (uint8_t)c;
            SDL_SetRenderDrawColor(vs->renderer, r, g, b, a);
            SDL_RenderClear(vs->renderer);
            pc += 5;
        } else if (op == 0x02) { // Present flags
            if (pc + 2 > len) {
                ok = false;
                break;
            }
            const uint8_t flags = buf[pc + 1];
            const int tmp = front;
            front = back;
            back = tmp;
            if (flags & 0x01) {
                *wait_vbl = true;
            }
            pc += 2;
            SDL_SetRenderTarget(vs->renderer, display[back]);
        } else if (op == 0x04) { // DrawTexture handle, x, y
            if (pc + 7 > len) {
                ok = false;
                break;
            }
            const uint16_t handle = (uint16_t)buf[pc + 1] | ((uint16_t)buf[pc + 2] << 8);
            const int16_t x = (int16_t)((uint16_t)buf[pc + 3] | ((uint16_t)buf[pc + 4] << 8));
            const int16_t y = (int16_t)((uint16_t)buf[pc + 5] | ((uint16_t)buf[pc + 6] << 8));
            Slot *s = slot_of(handle);
            if (!s) {
                ok = false;
                break;
            }
            SDL_FRect dst = {(float)x, (float)y, (float)s->w, (float)s->h};
            SDL_RenderTexture(vs->renderer, s->tex, nullptr, &dst);
            pc += 7;
        } else {
            ok = false;
            break;
        }
    }

    SDL_SetRenderTarget(vs->renderer, old_target);
    if (clip_on) {
        SDL_SetRenderClipRect(vs->renderer, &clip_i);
    } else {
        SDL_SetRenderClipRect(vs->renderer, nullptr);
    }
    SDL_SetRenderDrawColor(vs->renderer, cr, cg, cb, ca);
    if (ok && *wait_vbl) {
        vbl_pending = true;
    }
    return ok;
}

void SsGpu::on_frame() {
    /* VBL edge for Present wait is observed via take_vbl_complete. */
}

bool SsGpu::take_vbl_complete() {
    if (!vbl_pending) {
        return false;
    }
    vbl_pending = false;
    return true;
}

bool SsGpu::frame_to_window() {
    if (!active || !vs || !display[front]) {
        return true;
    }
    SDL_FRect src = {0.0f, 0.0f, (float)width, (float)height};
    vs->render_frame(display[front], &src, nullptr, true, nullptr);
    return true;
}
