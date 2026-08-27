#pragma once

#include <cstdint>
#include <vector>
#include <SDL3/SDL.h>
#include "videosystem.hpp"
#include "devices/displaypp/RGBA.hpp"

/** Guest texture format bytes (SecondSight_GPU.md §5). */
enum ss_gpu_format_t : uint8_t {
    SS_GPU_FMT_IDX8 = 0x01,
    SS_GPU_FMT_RGB555 = 0x02,
    SS_GPU_FMT_RGB888 = 0x03,
    SS_GPU_FMT_ARGB8888 = 0x04,
};

struct ss_gpu_info_t {
    uint8_t isa_version = 1;
    uint32_t heap_size = 0;
    uint32_t heap_free = 0;
    uint16_t max_textures = 0;
    uint16_t max_csb = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t native_format = 0;
    uint8_t present_policy = 0;
    uint8_t active = 0;
};

class SsGpu {
public:
    static constexpr uint32_t HEAP_SIZE = 16u * 1024u * 1024u;
    static constexpr uint16_t MAX_TEXTURES = 256;
    static constexpr uint32_t MAX_CSB = 65536;
    static constexpr uint16_t INVALID_HANDLE = 0xFFFF;

    void init(video_system_t *vs);
    void shutdown();

    bool enter_mode(uint16_t w, uint16_t h, uint8_t native_format);
    void leave_mode();
    bool is_active() const { return active; }

    uint16_t upload_texture(uint16_t w, uint16_t h, uint8_t format, uint8_t flags,
        const uint8_t *pixels, uint32_t nbytes);
    bool free_texture(uint16_t handle);

    /** Interpret CSB. Returns false on error. If *wait_vbl, caller delays A5 until on_frame(). */
    bool exec_csb(const uint8_t *buf, uint32_t len, bool *wait_vbl);

    void on_frame();
    bool take_vbl_complete();
    bool frame_to_window();

    void fill_info(ss_gpu_info_t *out) const;

    uint8_t *ensure_upload_buf(uint32_t nbytes);
    uint8_t *csb_buf() { return csb.data(); }
    uint32_t last_csb_op() const { return last_op; }
    int handle_count() const;

    uint16_t mode_w() const { return width; }
    uint16_t mode_h() const { return height; }
    int front_index() const { return front; }
    int back_index() const { return back; }

private:
    struct Slot {
        SDL_Texture *tex = nullptr;
        uint16_t w = 0;
        uint16_t h = 0;
        uint8_t format = 0;
        uint8_t flags = 0;
        uint32_t bytes = 0;
    };

    video_system_t *vs = nullptr;
    SDL_Texture *display[2] = {nullptr, nullptr};
    int front = 0;
    int back = 1;
    std::vector<Slot> slots; // [0] unused (display handle)
    std::vector<uint8_t> upload;
    std::vector<uint8_t> csb;
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t native_format = 0;
    uint8_t present_policy = 0;
    bool active = false;
    bool vbl_pending = false;
    uint32_t heap_used = 0;
    uint32_t last_op = 0;

    static uint32_t bpp_of(uint8_t format);
    static bool convert_to_rgba(uint8_t format, uint16_t w, uint16_t h,
        const uint8_t *src, uint32_t nbytes, std::vector<uint8_t> &out);
    SDL_Texture *make_rgba_texture(uint16_t w, uint16_t h, const uint8_t *rgba);
    void destroy_display();
    void destroy_slot(Slot &s);
    uint16_t alloc_handle();
    Slot *slot_of(uint16_t handle);
};
