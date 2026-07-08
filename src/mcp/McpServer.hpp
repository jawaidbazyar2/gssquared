/*
 *   Copyright (c) 2026 GSSquared contributors
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
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

struct computer_t;
struct cpu_state;

namespace mcp {

// McpServer: exposes the running emulator to an MCP client (e.g. Claude
// Code) as JSON-RPC 2.0 tools, modelled on ~/src/verilogapple's MCP
// server. Newline-delimited JSON-RPC over a UNIX domain socket; a `socat`
// bridge in .mcp.json connects the client's stdio to the socket.
//
// Sibling to the Agent (src/agent/) and independent of it: this drives the
// existing debugger / cpu_state / MMU directly. Enable by setting
// GS2_MCP_SOCKET to a socket path; stays absent otherwise.
//
// Threading: an IO thread owns the socket and parses requests. Any tool
// that reads or mutates emulator state is marshalled onto the emulator
// thread via a command queue that pump() drains once per frame, so
// cpu_state / MMU are never touched concurrently with the CPU.
class McpServer {
public:
    struct Config {
        std::string socket_path;
    };

    McpServer(Config cfg, computer_t *computer);
    ~McpServer();

    McpServer(const McpServer &) = delete;
    McpServer &operator=(const McpServer &) = delete;

    // True once the listening socket is open and the IO thread is running.
    bool enabled() const { return enabled_.load(std::memory_order_acquire); }

    // Called once per frame from the emulator thread (run_one_frame).
    // Runs any queued tool commands here, where touching cpu_state / MMU
    // is race-free.
    void pump();

private:
    using json = nlohmann::json;

    void io_thread_main();
    void serve_client(int client_fd);

    // Dispatch one parsed JSON-RPC message. Returns the response object,
    // or a null json for notifications (which get no reply).
    json handle_message(const json &req);
    json handle_tool_call(const std::string &name, const json &args);
    json tools_catalogue();

    // Run `fn` on the emulator thread (via pump) and block the IO thread
    // until it completes. Returns false on timeout (e.g. no system is
    // running yet, so pump() isn't being called).
    bool call_on_emulator(std::function<json()> fn, json &out, int timeout_ms);

    // -- Tool bodies. These run on the emulator thread only. --
    json tool_regs();
    json tool_peek(uint32_t addr, uint32_t len);
    json tool_poke(uint32_t addr, const std::vector<uint8_t> &bytes);
    json tool_reset(bool cold);
    json tool_step(uint32_t count);
    json tool_until_pc(uint32_t target, uint64_t max_insns);
    json tool_disasm(uint32_t addr, uint32_t count);
    json tool_screen_text();
    json tool_mem_diff(const std::string &action, uint32_t addr, uint32_t len);
    json tool_setreg(const std::string &reg, uint32_t value);
    json tool_type(const std::string &text);
    json tool_wait_input(uint64_t max_insns);
    json tool_mount_disk(int slot, int drive, const std::string &filename);
    json tool_unmount_disk(int slot, int drive);
    json tool_set_mode(int mode);  // 0=run, 1=step, 2=paused

    // Render the current frame to a PNG file and return its path. Works in
    // both headed and headless modes (headless uses the software renderer).
    json tool_screenshot(const std::string &path);
    // Inject mouse/keyboard input the way the agent socket does — by pushing
    // SDL events stamped with the agent sentinel id so keygloo/ADB consume
    // them. Work in both modes.
    json tool_mouse_move(int dx, int dy);
    json tool_mouse_button(int button, bool down);
    json tool_mouse_mode(int mode);  // 0=follow_host, 1=capture, 2=disabled

    // Inject an SDL key-down/up event (via keygloo -> ADB keyboard) so machines
    // without a classic keyboard module (Apple IIgs) can be typed on.
    void push_key_event(int scancode, bool shift, bool down);
    // `type` fallback for keygloo/ADB machines: map ASCII to SDL scancodes and
    // inject key events.
    json type_via_sdl_keys(const std::string &text);

    // Drive the CPU until the scanner emits its next VSYNC (frame boundary).
    // Used by the screenshot tool to render complete, aligned frames on demand.
    bool drive_to_next_vsync();

    // Execute exactly one instruction the way the normal run loop does:
    // service due timers, then let the CPU core advance the clock/scanner.
    void drive_one(cpu_state *cpu);

    // Finish a synchronous CPU-driving tool. MCP can run many emulated
    // frames inside one host frame, so discard stale scan samples before the
    // GUI renderer consumes them and catch the clock's frame window up to the
    // current emulated time.
    void finish_driven_batch();

    // mem_diff snapshot state. Only touched from emulator-thread closures
    // (which run serially via pump), so no extra locking is needed.
    uint32_t snap_addr_ = 0;
    std::vector<std::uint8_t> snap_;

    Config cfg_;
    computer_t *computer_;
    int listen_fd_ = -1;
    // fd of the currently-connected client, or -1. Atomic so the destructor
    // can shutdown() it to interrupt a blocking recv() in the IO thread —
    // closing the listen socket alone does not unblock an accepted client,
    // so without this a teardown while a client is attached hangs on join().
    std::atomic<int> client_fd_{-1};
    std::thread io_thread_;
    std::atomic<bool> enabled_{false};
    std::atomic<bool> shutdown_{false};

    std::mutex queue_mu_;
    std::deque<std::function<void()>> queue_;

    // Incremented once per pump() (i.e. per emulated frame). Used by
    // call_on_emulator to tell "the emulation loop is alive" from "no
    // session / stalled" without imposing a wall-clock cap on how long a
    // tool may run — long CPU-driving tools are bounded by their own
    // instruction budget instead.
    std::atomic<std::uint64_t> pump_beats_{0};
};

}  // namespace mcp
