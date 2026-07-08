#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "computer.hpp"
#include "cpu.hpp"
#include "videosystem.hpp"
#include "mmus/mmu_ii.hpp"
#include "debug.hpp"

#include "devices/adb/keygloo_state.hpp"
#include "devices/adb/ADB_Micro.hpp"

namespace keygloo_mouse_sync {

// Set to 1 while diagnosing EM mouse sync (also enable with debug_level |= DEBUG_MOUSE).
#ifndef KEYGLOO_MOUSE_SYNC_TRACE
#define KEYGLOO_MOUSE_SYNC_TRACE 0
#endif

// Set to 1 to select the closed-loop injection sync path: instead of patching EM
// coordinate globals ($047C etc.), the guest's live EM position is read back each
// frame as feedback and a single bounded ADB relative-motion step is injected until
// it converges on the host-derived target. Flag off keeps the original KEGS-style
// MMU-write (dead-reckoning) path.
#ifndef KEYGLOO_MOUSE_SYNC_INJECTION_ONLY
#define KEYGLOO_MOUSE_SYNC_INJECTION_ONLY 1
#endif

inline bool sync_trace_enabled() {
    return KEYGLOO_MOUSE_SYNC_TRACE || DEBUG(DEBUG_MOUSE);
}

// Closed-loop injection path is active when the flag is set; otherwise the KEGS
// MMU-write path (em_memory_patch_enabled) is used.
inline bool closed_loop_enabled() {
    return KEYGLOO_MOUSE_SYNC_INJECTION_ONLY != 0;
}

inline bool em_memory_patch_enabled() {
    return !KEYGLOO_MOUSE_SYNC_INJECTION_ONLY;
}

static constexpr uint32_t GS_RAM_SIZE = 8u * 1024u * 1024u;
static constexpr uint32_t TOOL_LOCATOR_LO = 0x103C8;
static constexpr int EM_TOOL_ACTIVE_OFFSET = 6 * 4;
static constexpr uint32_t EM_MOUSE_X_LO = 0x047C;
static constexpr uint32_t EM_MOUSE_X_HI = 0x057C;
static constexpr uint32_t EM_MOUSE_Y_LO = 0x04FC;
static constexpr uint32_t EM_MOUSE_Y_HI = 0x05FC;
static constexpr uint32_t EM_MOUSE_X_LO_BANK = 0x10190;
static constexpr uint32_t EM_MOUSE_X_HI_BANK = 0x10192;
static constexpr uint32_t EM_MOUSE_Y_LO_BANK = 0x10191;
static constexpr uint32_t EM_MOUSE_Y_HI_BANK = 0x10193;
static constexpr uint32_t SHR_SCB_LINE0 = 0x19D00;
static constexpr int MOTION_CHUNK_MAX = 63;

inline int clamp_int(int value, int lo, int hi) {
    return std::max(lo, std::min(value, hi));
}

inline uint8_t read_megaii_linear(MMU_II *megaii, uint32_t addr) {
    return megaii->get_memory_base()[addr & 0x1FFFF];
}

inline bool shr_320_mode(MMU_II *megaii) {
    if (!megaii) {
        return false;
    }
    // KEGS adb_update_mouse(): line 0 SCB bit 7 clear => 320-column SHR desktop.
    return (read_megaii_linear(megaii, SHR_SCB_LINE0) & 0x80) == 0;
}

inline void read_em_mouse_pos(MMU_II *mmu, int &x, int &y) {
    uint8_t x_lo = read_megaii_linear(mmu, EM_MOUSE_X_LO);
    uint8_t x_hi = read_megaii_linear(mmu, EM_MOUSE_X_HI);
    uint8_t y_lo = read_megaii_linear(mmu, EM_MOUSE_Y_LO);
    uint8_t y_hi = read_megaii_linear(mmu, EM_MOUSE_Y_HI);
    x = x_lo | (x_hi << 8);
    y = y_lo | (y_hi << 8);
}

inline void write_em_axis(keygloo_state_t *kb_state, uint32_t lo, uint32_t hi,
        uint32_t bank_lo, uint32_t bank_hi, int value) {
    if (!em_memory_patch_enabled()) {
        return;
    }
    if (!kb_state || !kb_state->mmu || !kb_state->computer || !kb_state->computer->cpu ||
        !kb_state->computer->cpu->mmu) {
        return;
    }

    uint8_t byte_lo = static_cast<uint8_t>(value & 0xFF);
    uint8_t byte_hi = static_cast<uint8_t>((value >> 8) & 0xFF);
    MMU_II *megaii = kb_state->mmu;
    megaii->get_memory_base()[lo & 0x1FFFF] = byte_lo;
    megaii->get_memory_base()[hi & 0x1FFFF] = byte_hi;

    MMU *fpi = kb_state->computer->cpu->mmu;
    fpi->write(lo, byte_lo);
    fpi->write(hi, byte_hi);
    fpi->write(bank_lo, byte_lo);
    fpi->write(bank_hi, byte_hi);
}

inline const SDL_FRect &guest_content_rect(video_system_t *vs) {
    if (vs && vs->content.w > 0.0f && vs->content.h > 0.0f) {
        return vs->content;
    }
    return vs->target;
}

inline bool render_point_in_guest_content(video_system_t *vs, float rx, float ry) {
    const SDL_FRect &content = guest_content_rect(vs);
    if (content.w <= 0.0f || content.h <= 0.0f) {
        return false;
    }
    return rx >= content.x && rx < (content.x + content.w) &&
        ry >= content.y && ry < (content.y + content.h);
}

inline void host_to_a2(computer_t *computer, float render_x, float render_y, int &ax, int &ay) {
    video_system_t *vs = computer->video_system;
    const SDL_FRect &content = guest_content_rect(vs);

    float norm_x = 0.0f;
    float norm_y = 0.0f;
    if (content.w > 0.0f) {
        norm_x = (render_x - content.x) / content.w;
    }
    if (content.h > 0.0f) {
        norm_y = (render_y - content.y) / content.h;
    }

    int px = (int)std::lround(norm_x * 639.0f);
    int py = (int)std::lround(norm_y * 399.0f);
    px = clamp_int(px, 0, 639);
    py = clamp_int(py, 0, 399);
    ay = py >> 1;
    ax = px;
    if (computer && computer->mmu && shr_320_mode(computer->mmu)) {
        ax = ax >> 1;
    }
}

inline void window_to_render_coords(video_system_t *vs, float wx, float wy, float &rx, float &ry) {
    rx = wx;
    ry = wy;
    if (vs && vs->renderer) {
        SDL_RenderCoordinatesFromWindow(vs->renderer, wx, wy, &rx, &ry);
    }
}

inline std::vector<MotionChunk> split_motion_delta(int dx, int dy) {
    std::vector<MotionChunk> chunks;
    while (dx != 0 || dy != 0) {
        int chunk_dx = dx;
        int chunk_dy = dy;
        if (chunk_dx > MOTION_CHUNK_MAX) {
            chunk_dx = MOTION_CHUNK_MAX;
        } else if (chunk_dx < -MOTION_CHUNK_MAX) {
            chunk_dx = -MOTION_CHUNK_MAX;
        }
        if (chunk_dy > MOTION_CHUNK_MAX) {
            chunk_dy = MOTION_CHUNK_MAX;
        } else if (chunk_dy < -MOTION_CHUNK_MAX) {
            chunk_dy = -MOTION_CHUNK_MAX;
        }
        chunks.push_back({chunk_dx, chunk_dy});
        dx -= chunk_dx;
        dy -= chunk_dy;
    }
    return chunks;
}

inline bool compute_em_active(keygloo_state_t *kb_state) {
    if (!kb_state->mmu || !kb_state->computer || !kb_state->computer->cpu ||
        !kb_state->computer->cpu->mmu) {
        return false;
    }

    // Tool Locator pointer at $103C8 lives in MegaII RAM (computer->mmu).
    // The pointer it yields is a 24-bit FPI address; read the EM active word
    // through cpu->mmu so bank shadowing is respected.
    MMU_II *megaii = kb_state->mmu;
    uint32_t tool_start =
        (static_cast<uint32_t>(read_megaii_linear(megaii, TOOL_LOCATOR_LO + 2)) << 16) |
        (static_cast<uint32_t>(read_megaii_linear(megaii, TOOL_LOCATOR_LO + 1)) << 8) |
        static_cast<uint32_t>(read_megaii_linear(megaii, TOOL_LOCATOR_LO));

    bool active = false;
    if (tool_start >= 0x20000 && tool_start < (GS_RAM_SIZE - 28)) {
        MMU *fpi = kb_state->computer->cpu->mmu;
        uint8_t active_lo = fpi->read(tool_start + EM_TOOL_ACTIVE_OFFSET);
        uint8_t active_hi = fpi->read(tool_start + EM_TOOL_ACTIVE_OFFSET + 1);
        active = (active_lo | (active_hi << 8)) != 0;
    }

    if (kb_state->computer->video_system &&
        kb_state->computer->video_system->mouse_captured) {
        active = false;
    }

    return active;
}

} // namespace keygloo_mouse_sync

inline KeyGloo::~KeyGloo() = default;

inline void KeyGloo::set_host_context(keygloo_state_t *ctx) {
    host_ctx = ctx;
}

inline void KeyGloo::detect_and_update_em_active() {
    if (!host_ctx) {
        return;
    }

    bool prev = em_active;
    bool new_active = keygloo_mouse_sync::compute_em_active(host_ctx);
        if (new_active != prev) {
        printf("KeyGloo: Event Manager %s\n", new_active ? "active" : "inactive");
        if (new_active) {
            if (keygloo_mouse_sync::closed_loop_enabled()) {
                // Closed-loop stepper reads live EM position each frame; just
                // clear any stale stepper state on activation.
                pending_target = false;
                step_outstanding = false;
                stale_frames = 0;
                printf("KeyGloo: EM sync closed-loop injection (no MMU coordinate patches)\n");
            } else {
                seed_reported_from_em();
            }
        } else {
            reported_valid = false;
            motion_queue.clear();
            pending_target = false;
            step_outstanding = false;
            stale_frames = 0;
            restore_em_host_cursor();
        }
    }
    em_active = new_active;
}

inline void KeyGloo::seed_reported_from_em() {
    if (!host_ctx) {
        return;
    }
    keygloo_mouse_sync::read_em_mouse_pos(host_ctx->mmu, reported_x, reported_y);
    reported_valid = true;
    motion_queue.clear();
    if (keygloo_mouse_sync::sync_trace_enabled()) {
        printf("KeyGloo sync: seed reported (%d,%d)\n", reported_x, reported_y);
    }
}


inline void KeyGloo::step_em_closed_loop() {
    if (!keygloo_mouse_sync::closed_loop_enabled()) {
        return;
    }
    if (!host_ctx || !em_active || !pending_target || !adb_mouse) {
        return;
    }

    static constexpr int DEADBAND = 1;
    static constexpr int STALE_LIMIT = 4;

    int ex = 0;
    int ey = 0;
    keygloo_mouse_sync::read_em_mouse_pos(host_ctx->mmu, ex, ey);

    // Converged: within the deadband of the target, stop stepping.
    if (std::abs(target_x - ex) <= DEADBAND && std::abs(target_y - ey) <= DEADBAND) {
        pending_target = false;
        step_outstanding = false;
        stale_frames = 0;
        return;
    }

    // The previous injected event has not been fully read by the guest yet.
    if (mouse_data_full) {
        return;
    }

    // Wait for the guest EM to actually post the previous step before injecting
    // again, otherwise we would over-inject against a stale position (overshoot).
    if (step_outstanding && ex == em_snapshot_x && ey == em_snapshot_y) {
        stale_frames++;
        if (stale_frames < STALE_LIMIT) {
            return;
        }
        // Gave up waiting: assume the step was dropped/no-op and re-inject.
    }

    int dx = keygloo_mouse_sync::clamp_int(target_x - ex,
        -keygloo_mouse_sync::MOTION_CHUNK_MAX, keygloo_mouse_sync::MOTION_CHUNK_MAX);
    int dy = keygloo_mouse_sync::clamp_int(target_y - ey,
        -keygloo_mouse_sync::MOTION_CHUNK_MAX, keygloo_mouse_sync::MOTION_CHUNK_MAX);
    if (dx == 0 && dy == 0) {
        pending_target = false;
        step_outstanding = false;
        stale_frames = 0;
        return;
    }

    em_snapshot_x = ex;
    em_snapshot_y = ey;
    stale_frames = 0;
    step_outstanding = true;

    if (!adb_mouse->inject_relative_motion(dx, dy)) {
        step_outstanding = false;
        return;
    }
    deliver_mouse_reg0();

    if (keygloo_mouse_sync::sync_trace_enabled()) {
        printf("KeyGloo sync: closed-loop step EM (%d,%d) target (%d,%d) inject (%d,%d)\n",
            ex, ey, target_x, target_y, dx, dy);
    }
}

inline void KeyGloo::deliver_mouse_reg0() {
    ADB_Register reg;
    adb_host->talk(0x03, 0b11, 0, reg);
    mouse_data[0] = reg.data[1];
    mouse_data[1] = reg.data[0];
    mouse_data_full = true;
    mouse_x_available = MOUSE_X;
    update_interrupt_status();
}

inline void KeyGloo::drain_motion_queue() {
    if (mouse_data_full || motion_queue.empty() || !adb_mouse) {
        return;
    }

    MotionChunk chunk = motion_queue.front();
    motion_queue.erase(motion_queue.begin());

    if (!adb_mouse->inject_relative_motion(chunk.dx, chunk.dy)) {
        return;
    }
    pending_dx = chunk.dx;
    pending_dy = chunk.dy;
    deliver_mouse_reg0();
    if (keygloo_mouse_sync::sync_trace_enabled()) {
        printf("KeyGloo sync: inject chunk (%d,%d) queue_remain=%zu mouse_data=(%02X,%02X)\n",
            chunk.dx, chunk.dy, motion_queue.size(), mouse_data[0], mouse_data[1]);
    }
}

inline void KeyGloo::restore_em_host_cursor() {
    if (em_host_cursor_hidden) {
        SDL_ShowCursor();
        em_host_cursor_hidden = false;
    }
}

inline void KeyGloo::update_em_host_cursor(float wx, float wy) {
    if (!host_ctx) {
        restore_em_host_cursor();
        return;
    }

    video_system_t *vs = host_ctx->computer->video_system;
    if (vs->osd_control_panel_open) {
        restore_em_host_cursor();
        return;
    }

    if (!em_active) {
        restore_em_host_cursor();
        return;
    }

    float rx = wx;
    float ry = wy;
    keygloo_mouse_sync::window_to_render_coords(vs, wx, wy, rx, ry);
    const bool inside = keygloo_mouse_sync::render_point_in_guest_content(vs, rx, ry);

    if (inside && !em_host_cursor_hidden) {
        SDL_HideCursor();
        em_host_cursor_hidden = true;
    } else if (!inside && em_host_cursor_hidden) {
        SDL_ShowCursor();
        em_host_cursor_hidden = false;
    }
}

inline void KeyGloo::handle_em_mouse_motion(float wx, float wy) {
    if (!host_ctx) {
        return;
    }

    update_em_host_cursor(wx, wy);

    video_system_t *vs = host_ctx->computer->video_system;

    float rx = wx;
    float ry = wy;
    keygloo_mouse_sync::window_to_render_coords(vs, wx, wy, rx, ry);

    if (!keygloo_mouse_sync::render_point_in_guest_content(vs, rx, ry)) {
        if (keygloo_mouse_sync::sync_trace_enabled()) {
            const SDL_FRect &content = keygloo_mouse_sync::guest_content_rect(vs);
            printf("KeyGloo sync: SDL window (%.0f,%.0f) render (%.0f,%.0f) outside content "
                "(%.0f,%.0f %.0fx%.0f)\n",
                wx, wy, rx, ry, content.x, content.y, content.w, content.h);
        }
        return;
    }

    keygloo_mouse_sync::host_to_a2(host_ctx->computer, rx, ry, target_x, target_y);

    if (keygloo_mouse_sync::closed_loop_enabled()) {
        // Closed-loop mode only records the target here; the per-frame stepper
        // (step_em_closed_loop) reads the guest's live EM position and injects.
        pending_target = true;
        if (keygloo_mouse_sync::sync_trace_enabled()) {
            printf("KeyGloo sync: closed-loop target (%d,%d) from SDL (%.0f,%.0f)\n",
                target_x, target_y, wx, wy);
        }
        return;
    }

    if (mouse_data_full) {
        if (keygloo_mouse_sync::sync_trace_enabled()) {
            printf("KeyGloo sync: SDL window (%.0f,%.0f) render (%.0f,%.0f) "
                "target (%d,%d) pending guest read\n",
                wx, wy, rx, ry, target_x, target_y);
        }
        return;
    }

    if (!reported_valid) {
        seed_reported_from_em();
    }

    int total_dx = target_x - reported_x;
    int total_dy = target_y - reported_y;
    if (total_dx == 0 && total_dy == 0) {
        motion_queue.clear();
        return;
    }

    motion_queue = keygloo_mouse_sync::split_motion_delta(total_dx, total_dy);

    if (keygloo_mouse_sync::sync_trace_enabled()) {
        const SDL_FRect &content = keygloo_mouse_sync::guest_content_rect(vs);
        printf("KeyGloo sync: SDL window (%.0f,%.0f) render (%.0f,%.0f) content (%.0f,%.0f %.0fx%.0f) "
            "A2 target (%d,%d) reported (%d,%d) delta (%d,%d) chunks=%zu:",
            wx, wy, rx, ry, content.x, content.y, content.w, content.h,
            target_x, target_y, reported_x, reported_y, total_dx, total_dy, motion_queue.size());
        for (size_t i = 0; i < motion_queue.size(); i++) {
            printf(" (%d,%d)", motion_queue[i].dx, motion_queue[i].dy);
        }
        printf("\n");
    }

    drain_motion_queue();
}

inline uint8_t KeyGloo::read_mouse_data() {
    const bool closed_loop = keygloo_mouse_sync::closed_loop_enabled();

    if (mouse_x_available == MOUSE_Y) {
        mouse_data_full = false;
        mouse_x_available = MOUSE_X;
        update_interrupt_status();

        return mouse_data[0];
    }

    mouse_x_available = MOUSE_Y;
    update_interrupt_status();
    return mouse_data[1];
}
