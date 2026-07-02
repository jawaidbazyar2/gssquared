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
    json tool_mount_disk(int slot, int drive, const std::string &filename);
    json tool_unmount_disk(int slot, int drive);
    json tool_set_mode(int mode);  // 0=run, 1=step, 2=paused

    // mem_diff snapshot state. Only touched from emulator-thread closures
    // (which run serially via pump), so no extra locking is needed.
    uint32_t snap_addr_ = 0;
    std::vector<std::uint8_t> snap_;

    Config cfg_;
    computer_t *computer_;
    int listen_fd_ = -1;
    std::thread io_thread_;
    std::atomic<bool> enabled_{false};
    std::atomic<bool> shutdown_{false};

    std::mutex queue_mu_;
    std::deque<std::function<void()>> queue_;
};

}  // namespace mcp
