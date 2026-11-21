/*
 * Decide when the browser frame loop in gs2.cpp should run or catch up.
 * Deadlines use host nanoseconds from SDL_GetTicksNS; durations also use
 * nanoseconds. The frame loop advances the emulated CPU clocks separately.
 * The browser's main thread calls these pure helpers; they retain no state.
 * The caller keeps the next frame deadline and estimates the next frame cost.
 * These limits are responsiveness policies, not Apple timing.
 */
#pragma once

#include <cstdint>

namespace web_frame_pacing {
// Admit another catch-up frame only if its estimated cost fits in 24 ms.
// This is a batch admission limit, not a deadline that interrupts execution;
// gs2.cpp also yields during a frame to give browser input and drawing time.
constexpr uint64_t batch_budget_ns = 24000000;
// After a quarter-second delay, discard most wall-clock debt so returning
// from a suspended tab does not start an extended catch-up burst.
constexpr uint64_t suspension_ns = 250000000;
// Ignore up to 2 ms of callback jitter when deciding to enter catch-up mode.
constexpr uint64_t jitter_grace_ns = 2000000;
// Even very cheap frames must return control after two attempts per batch.
constexpr unsigned max_frames = 2;

// Return true when the next frame is due at now. A zero deadline allows
// the first frame to start before the caller establishes its pacing schedule.
constexpr bool due(uint64_t deadline, uint64_t now) {
    return !deadline || now >= deadline;
}
// Return true when a scheduled frame is late enough to warrant catch-up.
// A zero deadline has no accumulated debt, even though due() allows its
// first frame to run.
constexpr bool needs_catch_up(uint64_t deadline, uint64_t now) {
    return deadline && now > deadline && now - deadline > jitter_grace_ns;
}
// Return a replacement deadline after a long pause. period is one guest
// frame's normal wall-clock duration, in nanoseconds. Retain four periods of
// debt for bounded catch-up; the caller still executes every guest frame.
// This changes host pacing only. It does not advance or skip emulated time.
constexpr uint64_t bound_debt(uint64_t deadline, uint64_t now, uint64_t period) {
    return deadline && now > deadline && now - deadline > suspension_ns
        ? now - 4 * period : deadline;
}
// Return true when the caller may run one more frame. frames counts frames
// already completed in this batch. elapsed is the batch's wall time and
// estimate is the caller's expected cost for another frame, both in nanoseconds.
// Check elapsed first to keep subtraction unsigned-safe.
constexpr bool another_frame(unsigned frames, uint64_t elapsed, uint64_t estimate) {
    return frames < max_frames && elapsed <= batch_budget_ns &&
        estimate <= batch_budget_ns - elapsed;
}
// One browser task runs Apple instructions for at most 5 ms, then yields so
// the browser can draw and run audio. Without this limit, a frame that takes
// longer than a browser frame blocks both.
constexpr uint64_t slice_budget_ns = 5000000;
// Instructions between two reads of the host clock inside a slice.
constexpr unsigned slice_check_interval = 256;
}

// The browser frame loop's state. The browser's main thread owns it. A frame
// that stops at the end of a slice is a partial frame: it keeps its starting
// CPU count and its end-of-frame work for the task that finishes it.
struct web_frame_loop_t {
    // Host time the next guest frame is due; zero before a schedule exists.
    uint64_t target_start_ns = 0;
    // Host time the current slice ends; zero runs a frame to its end.
    uint64_t slice_deadline_ns = 0;
    unsigned slice_count = 0;
    bool partial_frame = false;
    uint64_t frame_cpu_start = 0;
    // Draw once after a batch of frames, not after each of its frames.
    bool defer_present = false;
    bool frame_rendered = false;
    bool hidden = false;
    bool was_batchable = false;
    bool catching_up = false;
    bool replaying_events = false;
    // Cost of the last frame; one frame period until a frame is measured.
    uint64_t estimated_frame_ns = 16688154;

    void restart() {
        target_start_ns = 0;
        slice_deadline_ns = 0;
        partial_frame = false;
    }
    bool frame_due(uint64_t now) const { return web_frame_pacing::due(target_start_ns, now); }
    void begin_slice(uint64_t task_start) {
        slice_deadline_ns = task_start + web_frame_pacing::slice_budget_ns;
        slice_count = 0;
    }
    // host_ns reads the host clock; it is not called for every instruction.
    template<typename Clock> bool slice_expired(Clock host_ns) {
        return slice_deadline_ns && ++slice_count % web_frame_pacing::slice_check_interval == 0 &&
            host_ns() >= slice_deadline_ns;
    }
    // Start catch-up when a frame is late; end it when the next is not yet due.
    void note_lateness(uint64_t now) {
        if (web_frame_pacing::needs_catch_up(target_start_ns, now)) catching_up = true;
        else if (!web_frame_pacing::due(target_start_ns, now)) catching_up = false;
    }
};
