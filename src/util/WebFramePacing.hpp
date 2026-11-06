/*
 * Decide when the browser frame loop in gs2.cpp should run or catch up.
 * Deadlines use host nanoseconds from SDL_GetTicksNS; durations also use
 * nanoseconds. The frame loop advances the emulated CPU clocks separately.
 * The browser's main thread calls these pure helpers; they retain no state.
 * The caller keeps the next frame deadline and estimates the next frame cost.
 * These limits are responsiveness policies, not Apple or Second Sight timing.
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
}
