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

#include "mcp/McpServer.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "computer.hpp"
#include "cpu.hpp"
#include "debugger/disasm.hpp"
#include "mmus/mmu.hpp"

namespace mcp {

using json = nlohmann::json;

namespace {
constexpr int kCallTimeoutMs = 2000;

json text_result(const std::string &s, bool is_error = false) {
    json r;
    r["content"] = json::array({{{"type", "text"}, {"text", s}}});
    if (is_error) r["isError"] = true;
    return r;
}
}  // namespace

McpServer::McpServer(Config cfg, computer_t *computer)
    : cfg_(std::move(cfg)), computer_(computer) {
    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::fprintf(stderr, "[mcp] socket() failed: %s\n", std::strerror(errno));
        return;
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (cfg_.socket_path.size() >= sizeof(addr.sun_path)) {
        std::fprintf(stderr, "[mcp] socket path too long: %s\n", cfg_.socket_path.c_str());
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    std::strncpy(addr.sun_path, cfg_.socket_path.c_str(), sizeof(addr.sun_path) - 1);

    ::unlink(cfg_.socket_path.c_str());  // clear any stale socket
    if (::bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "[mcp] bind(%s) failed: %s\n", cfg_.socket_path.c_str(),
                     std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }
    if (::listen(listen_fd_, 1) != 0) {
        std::fprintf(stderr, "[mcp] listen() failed: %s\n", std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return;
    }

    enabled_.store(true, std::memory_order_release);
    std::fprintf(stderr, "[mcp] listening on %s\n", cfg_.socket_path.c_str());
    io_thread_ = std::thread(&McpServer::io_thread_main, this);
}

McpServer::~McpServer() {
    shutdown_.store(true, std::memory_order_release);
    if (listen_fd_ >= 0) {
        ::shutdown(listen_fd_, SHUT_RDWR);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (io_thread_.joinable()) io_thread_.join();
    if (!cfg_.socket_path.empty()) ::unlink(cfg_.socket_path.c_str());
}

void McpServer::io_thread_main() {
    while (!shutdown_.load(std::memory_order_acquire)) {
        int client_fd = ::accept(listen_fd_, nullptr, nullptr);
        if (client_fd < 0) {
            if (shutdown_.load(std::memory_order_acquire)) break;
            continue;
        }
        std::fprintf(stderr, "[mcp] client connected\n");
        serve_client(client_fd);
        ::close(client_fd);
        std::fprintf(stderr, "[mcp] client disconnected\n");
    }
}

void McpServer::serve_client(int client_fd) {
    std::string buf;
    char chunk[4096];
    while (!shutdown_.load(std::memory_order_acquire)) {
        ssize_t n = ::recv(client_fd, chunk, sizeof(chunk), 0);
        if (n <= 0) return;  // EOF or error
        buf.append(chunk, static_cast<size_t>(n));

        // Newline-delimited JSON-RPC: process each complete line.
        size_t nl;
        while ((nl = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, nl);
            buf.erase(0, nl + 1);
            if (line.empty()) continue;

            json response;
            try {
                json req = json::parse(line);
                response = handle_message(req);
            } catch (const std::exception &e) {
                response = {{"jsonrpc", "2.0"},
                            {"id", nullptr},
                            {"error", {{"code", -32700}, {"message", std::string("parse error: ") + e.what()}}}};
            }
            if (response.is_null()) continue;  // notification: no reply

            std::string out = response.dump() + "\n";
            size_t sent = 0;
            while (sent < out.size()) {
                ssize_t w = ::send(client_fd, out.data() + sent, out.size() - sent, 0);
                if (w <= 0) return;
                sent += static_cast<size_t>(w);
            }
        }
    }
}

json McpServer::handle_message(const json &req) {
    const std::string method = req.value("method", "");
    const bool is_notification = !req.contains("id");
    json id = req.contains("id") ? req["id"] : json(nullptr);

    json result;
    json error;

    if (method == "initialize") {
        const json &params = req.value("params", json::object());
        result["protocolVersion"] = params.value("protocolVersion", "2024-11-05");
        result["capabilities"] = {{"tools", {{"listChanged", false}}}};
        result["serverInfo"] = {{"name", "gssquared-mcp"}, {"version", "0.1"}};
    } else if (method == "notifications/initialized") {
        return json(nullptr);  // notification, no response
    } else if (method == "ping") {
        result = json::object();
    } else if (method == "tools/list") {
        result = tools_catalogue();
    } else if (method == "tools/call") {
        const json &params = req.value("params", json::object());
        const std::string name = params.value("name", "");
        const json args = params.value("arguments", json::object());
        result = handle_tool_call(name, args);
    } else {
        error = {{"code", -32601}, {"message", "method not found: " + method}};
    }

    if (is_notification) return json(nullptr);

    json response = {{"jsonrpc", "2.0"}, {"id", id}};
    if (!error.is_null()) {
        response["error"] = error;
    } else {
        response["result"] = result;
    }
    return response;
}

json McpServer::tools_catalogue() {
    json tools = json::array();
    tools.push_back({{"name", "regs"},
                     {"description", "Read the 65816 CPU registers."},
                     {"inputSchema", {{"type", "object"}, {"properties", json::object()}}}});
    tools.push_back({{"name", "peek"},
                     {"description", "Read bytes from the CPU's address space."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties",
                        {{"addr", {{"type", "integer"}, {"description", "24-bit address"}}},
                         {"len", {{"type", "integer"}, {"description", "byte count (default 16)"}}}}},
                       {"required", json::array({"addr"})}}}});
    tools.push_back({{"name", "poke"},
                     {"description", "Write bytes into the CPU's address space."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties",
                        {{"addr", {{"type", "integer"}, {"description", "24-bit address"}}},
                         {"bytes", {{"type", "array"}, {"items", {{"type", "integer"}}}, {"description", "byte values"}}}}},
                       {"required", json::array({"addr", "bytes"})}}}});
    tools.push_back({{"name", "reset"},
                     {"description", "Reset the machine."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties",
                        {{"cold", {{"type", "boolean"}, {"description", "cold start (default false)"}}}}}}}});
    tools.push_back({{"name", "step"},
                     {"description", "Execute N CPU instructions (default 1)."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties", {{"count", {{"type", "integer"}}}}}}}});
    tools.push_back({{"name", "until_pc"},
                     {"description", "Run instructions until the full 24-bit PC reaches addr (bounded)."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties",
                        {{"addr", {{"type", "integer"}, {"description", "24-bit target PC"}}},
                         {"max", {{"type", "integer"}, {"description", "max instructions (default 1000000)"}}}}},
                       {"required", json::array({"addr"})}}}});
    tools.push_back({{"name", "disasm"},
                     {"description", "Disassemble N instructions starting at addr (default PC, count 8)."},
                     {"inputSchema",
                      {{"type", "object"},
                       {"properties",
                        {{"addr", {{"type", "integer"}, {"description", "start address; omit for current PC"}}},
                         {"count", {{"type", "integer"}}}}}}}});
    tools.push_back({{"name", "pause"},
                     {"description", "Pause CPU execution."},
                     {"inputSchema", {{"type", "object"}, {"properties", json::object()}}}});
    tools.push_back({{"name", "resume"},
                     {"description", "Resume CPU execution at normal speed."},
                     {"inputSchema", {{"type", "object"}, {"properties", json::object()}}}});
    return {{"tools", tools}};
}

json McpServer::handle_tool_call(const std::string &name, const json &args) {
    json out;
    std::function<json()> body;

    if (name == "regs") {
        body = [this]() { return tool_regs(); };
    } else if (name == "peek") {
        uint32_t addr = args.value("addr", 0u);
        uint32_t len = args.value("len", 16u);
        body = [this, addr, len]() { return tool_peek(addr, len); };
    } else if (name == "poke") {
        uint32_t addr = args.value("addr", 0u);
        std::vector<uint8_t> bytes;
        if (args.contains("bytes") && args["bytes"].is_array()) {
            for (const auto &b : args["bytes"]) bytes.push_back(static_cast<uint8_t>(b.get<int>() & 0xFF));
        }
        body = [this, addr, bytes]() { return tool_poke(addr, bytes); };
    } else if (name == "reset") {
        bool cold = args.value("cold", false);
        body = [this, cold]() { return tool_reset(cold); };
    } else if (name == "step") {
        uint32_t count = args.value("count", 1u);
        body = [this, count]() { return tool_step(count); };
    } else if (name == "until_pc") {
        uint32_t target = args.value("addr", 0u);
        uint64_t max_insns = args.value("max", static_cast<uint64_t>(1000000));
        body = [this, target, max_insns]() { return tool_until_pc(target, max_insns); };
    } else if (name == "disasm") {
        // 0xFFFFFFFF sentinel = "use current PC" (resolved on the emulator thread).
        uint32_t addr = args.value("addr", 0xFFFFFFFFu);
        uint32_t count = args.value("count", 8u);
        body = [this, addr, count]() { return tool_disasm(addr, count); };
    } else if (name == "pause") {
        body = [this]() { return tool_set_mode(2); };
    } else if (name == "resume") {
        body = [this]() { return tool_set_mode(0); };
    } else {
        return text_result("unknown tool: " + name, /*is_error=*/true);
    }

    if (!call_on_emulator(std::move(body), out, kCallTimeoutMs)) {
        return text_result(
            "emulator did not respond within timeout — is a system running? "
            "(tools require an emulation session, not the system selector)",
            /*is_error=*/true);
    }
    return text_result(out.dump(2));
}

bool McpServer::call_on_emulator(std::function<json()> fn, json &out, int timeout_ms) {
    auto prom = std::make_shared<std::promise<json>>();
    std::future<json> fut = prom->get_future();
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        if (shutdown_.load(std::memory_order_acquire)) return false;
        queue_.push_back([fn = std::move(fn), prom]() { prom->set_value(fn()); });
    }
    if (fut.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready) {
        return false;
    }
    out = fut.get();
    return true;
}

void McpServer::pump() {
    std::deque<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lk(queue_mu_);
        if (queue_.empty()) return;
        local.swap(queue_);
    }
    for (auto &fn : local) fn();
}

// ---- Tool bodies (emulator thread only) ----

json McpServer::tool_regs() {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu) return {{"error", "no cpu"}};
    return {
        {"a", cpu->a},   {"x", cpu->x},       {"y", cpu->y},
        {"sp", cpu->sp}, {"d", cpu->d},       {"p", cpu->p},
        {"pc", cpu->pc}, {"pb", cpu->pb},     {"db", cpu->db},
        {"full_pc", cpu->full_pc},            {"halt", cpu->halt},
        {"execution_mode", static_cast<int>(computer_->execution_mode)},
    };
}

json McpServer::tool_peek(uint32_t addr, uint32_t len) {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu || !cpu->mmu) return {{"error", "no cpu/mmu"}};
    if (len == 0) len = 1;
    if (len > 4096) len = 4096;  // sanity clamp
    json bytes = json::array();
    for (uint32_t i = 0; i < len; ++i) {
        bytes.push_back(cpu->mmu->read((addr + i) & 0xFFFFFF));
    }
    return {{"addr", addr}, {"len", len}, {"bytes", bytes}};
}

json McpServer::tool_poke(uint32_t addr, const std::vector<uint8_t> &bytes) {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu || !cpu->mmu) return {{"error", "no cpu/mmu"}};
    for (size_t i = 0; i < bytes.size(); ++i) {
        cpu->mmu->write((addr + i) & 0xFFFFFF, bytes[i]);
    }
    return {{"addr", addr}, {"wrote", bytes.size()}};
}

json McpServer::tool_until_pc(uint32_t target, uint64_t max_insns) {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu || !cpu->cpun) return {{"error", "no cpu"}};
    // Step synchronously on the emulator thread until the PC lands on the
    // target or we hit the instruction budget / a halt. Bounded so a bad
    // target can't wedge the emulator thread.
    computer_->execution_mode = EXEC_STEP_INTO;
    computer_->instructions_left = 0;
    target &= 0xFFFFFF;
    uint64_t executed = 0;
    bool hit = (cpu->full_pc & 0xFFFFFF) == target;
    while (!hit && executed < max_insns) {
        if (cpu->halt != 0) break;
        (cpu->cpun->execute_next)(cpu);
        ++executed;
        hit = (cpu->full_pc & 0xFFFFFF) == target;
    }
    return {{"hit", hit}, {"executed", executed}, {"full_pc", cpu->full_pc}, {"halt", cpu->halt}};
}

json McpServer::tool_disasm(uint32_t addr, uint32_t count) {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu || !cpu->mmu) return {{"error", "no cpu/mmu"}};
    if (addr == 0xFFFFFFFFu) addr = cpu->full_pc & 0xFFFFFF;
    if (count == 0) count = 1;
    if (count > 256) count = 256;
    Disassembler dis(cpu->mmu, cpu->cpu_type);
    dis.setAddress(addr & 0xFFFFFF);
    std::vector<std::string> lines = dis.disassemble(static_cast<int>(count));
    json out = json::array();
    for (auto &l : lines) out.push_back(l);
    return {{"addr", addr}, {"lines", out}};
}

json McpServer::tool_reset(bool cold) {
    if (!computer_) return {{"error", "no computer"}};
    computer_->reset(cold);
    return {{"reset", true}, {"cold", cold}};
}

json McpServer::tool_step(uint32_t count) {
    cpu_state *cpu = computer_ ? computer_->cpu : nullptr;
    if (!cpu || !cpu->cpun) return {{"error", "no cpu"}};
    // Put the machine in step mode so it doesn't free-run between calls,
    // then execute `count` instructions synchronously right here on the
    // emulator thread.
    computer_->execution_mode = EXEC_STEP_INTO;
    computer_->instructions_left = 0;
    uint32_t done = 0;
    for (; done < count; ++done) {
        if (cpu->halt != 0) break;
        (cpu->cpun->execute_next)(cpu);
    }
    return {{"stepped", done}, {"pc", cpu->pc}, {"pb", cpu->pb}, {"full_pc", cpu->full_pc}};
}

json McpServer::tool_set_mode(int mode) {
    if (!computer_) return {{"error", "no computer"}};
    computer_->execution_mode = static_cast<execution_modes_t>(mode);
    if (mode == EXEC_NORMAL) computer_->instructions_left = 0;
    return {{"execution_mode", mode}};
}

}  // namespace mcp
