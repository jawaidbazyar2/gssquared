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
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "agent/EventQueue.hpp"

struct cpu_state;

namespace agent {

class UnixSocketTransport;

// Agent: observes the running emulator and ships protocol packets to a
// connected compositor over a UNIX domain socket.
//
// Construction policy:
//   - The Agent is owned by computer_t. If the caller decides not to
//     allocate one, computer->agent stays nullptr and all integration
//     points (e.g. the IRQ wrapper in display.cpp) skip cleanly.
//   - If allocated, the constructor opens the listening socket and starts
//     the IO thread. The IO thread blocks in accept() until a compositor
//     connects, then drains the event queue to it. On disconnect it goes
//     back to accept(); on shutdown it cleanly tears down.
//   - The destructor signals shutdown and joins the thread; safe to call
//     from any thread that owns the Agent (typically the main thread).
//
// Threading:
//   - emit_*() and submit() are called from the emulator thread. Cheap:
//     build a small vector, lock-and-push to a bounded queue, return.
//   - The IO thread owns the socket fds and drains the queue.
//   - Stats accessors are safe to call from any thread.
class Agent {
public:
    struct Config {
        // Path for the listening UNIX socket. Empty means agent is disabled
        // (callers should usually skip allocation entirely in that case).
        std::string socket_path;

        // Bounded queue capacity. Drop-oldest on overflow.
        std::size_t queue_capacity = 4096;
    };

    explicit Agent(Config cfg);
    ~Agent();

    Agent(const Agent&) = delete;
    Agent& operator=(const Agent&) = delete;

    // True once the listening socket is open and the IO thread is running.
    // False if start_listening failed during construction.
    bool enabled() const { return enabled_.load(std::memory_order_acquire); }

    // -- Event emitters. Called from the emulator thread. Cheap. --
    // Each builds a protocol packet and pushes it onto the queue.

    void emit_vbl();
    // CPU write to a video-relevant address (SHR/text/hires/aux/softswitch).
    // Caller is responsible for filtering — this is on the hot path of every
    // shadowed-RAM write and can fire ~200K times per second under heavy
    // graphics activity.
    void emit_mem_write(std::uint32_t addr_24bit, std::uint8_t data);

    // Emit a bulk memory dump as a single TAG_MEM_BLOB packet. Used for
    // initial-state snapshots — see set_on_client_connect(). Address is
    // the canonical 24-bit ($E0xxxx, $E1xxxx) destination; data is a raw
    // copy of `len` bytes from emulator memory.
    void emit_mem_blob(std::uint32_t addr_24bit,
                       const std::uint8_t* data, std::size_t len);

    // Emit one TAG_WINDOW_SNAPSHOT_ENTRY (per-window record from the
    // IIgs WindMgr's window list). Driven by the on_tool_return hook
    // for FrontWindow when wants_window_snapshot_ is set.
    void emit_window_snapshot_entry(std::uint32_t addr,
                                    std::int16_t top, std::int16_t left,
                                    std::int16_t bottom, std::int16_t right,
                                    const std::uint8_t* title_chars,
                                    std::uint8_t title_len);

    // Toolbox dispatcher entry/exit observers. Called from the CPU hot
    // path when `cpu->full_pc` matches `$E10000` (entry) or
    // `cpu->next_tool_return_pc` (exit). The agent reads the relevant
    // stack bytes via `cpu->mmu`, builds a TOOL_CALL/TOOL_RETURN
    // packet, manages an internal call-frame stack so nested calls
    // pair up correctly, and updates `cpu->next_tool_return_pc` to
    // point at whichever frame is now on top.
    void on_tool_call_entry(cpu_state* cpu);
    void on_tool_return(cpu_state* cpu);

    // Stats — readable from any thread.
    std::size_t dropped() const { return queue_.dropped(); }
    std::size_t depth() const { return queue_.depth(); }

    // Register a callback fired by the IO thread immediately after each
    // successful HELLO write to a newly-connected compositor. Used by the
    // emulator integration to push an initial-state snapshot (e.g., the
    // 32K SHR shadow) so a mid-run-attaching compositor doesn't have to
    // wait for the IIgs to repaint everything before its view is correct.
    //
    // Threading: the callback is invoked on the agent's IO thread. It can
    // call any of the emit_*() methods (those just enqueue packets and
    // are thread-safe). It MUST NOT block forever — anything it does
    // delays the first event packet to the new client.
    void set_on_client_connect(std::function<void(Agent&)> cb) {
        std::lock_guard<std::mutex> lk(connect_cb_mu_);
        on_client_connect_ = std::move(cb);
    }

private:
    void io_thread_main();
    // Per-client reader. Runs in a thread spawned by the IO thread for
    // each accepted client. Parses incoming compositor → agent packets
    // and dispatches them (currently just input injection via SDL_PushEvent).
    void input_reader_main();
    void submit(std::vector<std::uint8_t> packet);

    // Internal Toolbox call frame, pushed on entry and popped on exit
    // so nested tool calls pair up correctly.
    struct ToolCallFrame {
        std::uint32_t return_pc;     // 24-bit, what RTL will jump to
        std::uint16_t x_at_entry;    // selector saved at entry
        std::uint16_t sp_at_entry;   // SP at entry; offsets to read at exit
    };
    static constexpr std::size_t MAX_CALL_DEPTH = 16;
    ToolCallFrame call_stack_[MAX_CALL_DEPTH];
    std::size_t call_depth_ = 0;

    Config cfg_;
    EventQueue queue_;
    std::unique_ptr<UnixSocketTransport> transport_;
    std::thread io_thread_;
    std::atomic<bool> enabled_{false};

    // Sequence counter for packets that carry one. Monotonically increments
    // forever; the receiver uses it to detect drops. uint32 wraps at ~2 years
    // of 60Hz frames — fine.
    std::atomic<std::uint32_t> seq_{0};

    // Last absolute mouse position we injected. The IIgs ADB mouse path
    // accumulates *relative* xrel/yrel deltas, not absolute x/y, so we
    // turn the absolute coords the compositor sends us into deltas
    // against the previous injected position. Initialized to (0,0); the
    // first injected motion will produce a delta equal to the target.
    std::int32_t last_inject_x_ = 0;
    std::int32_t last_inject_y_ = 0;

    // Optional callback invoked from the IO thread after HELLO is
    // written to a newly-connected client. Mutex protects against the
    // emulator-thread setter racing with the IO thread reader.
    std::mutex connect_cb_mu_;
    std::function<void(Agent&)> on_client_connect_;

    // When a new compositor connects, this is set true so the next
    // observed FrontWindow ($150E) tool-return will trigger a walk of
    // the IIgs WindMgr's window list, emitting one
    // TAG_WINDOW_SNAPSHOT_ENTRY per window so the compositor can
    // populate panels for windows that existed before it attached.
    // Cleared after one successful walk. Atomic because it's set from
    // the IO thread (in the on_client_connect callback) and read/cleared
    // from the CPU thread (in on_tool_return).
    std::atomic<bool> wants_window_snapshot_{false};

public:
    // Request that a window-list snapshot be emitted on the next
    // observed FrontWindow tool-return. Safe to call from any thread.
    void request_window_snapshot() {
        wants_window_snapshot_.store(true, std::memory_order_release);
    }
};

}  // namespace agent
