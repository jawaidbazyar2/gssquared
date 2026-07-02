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
#include <string>
#include <vector>

namespace agent {

// UnixSocketTransport: server-side UNIX domain socket that accepts a single
// compositor at a time and writes framed packets to it.
//
// All public methods that talk to sockets (start_listening, accept_blocking,
// write_packet, disconnect_client) are intended to be called from the agent's
// IO thread. shutdown() is the only method designed to be called from another
// thread; it wakes accept_blocking() via a self-pipe and causes future
// accept_blocking() calls to return false immediately.
//
// The transport drops sequentially-received clients on the floor — only one
// compositor at a time. If a second connects while the first is active, it
// will sit in the listen queue until the first disconnects. This matches our
// model: there is exactly one compositor for one emulator.
class UnixSocketTransport {
public:
    explicit UnixSocketTransport(std::string path);
    ~UnixSocketTransport();

    UnixSocketTransport(const UnixSocketTransport&) = delete;
    UnixSocketTransport& operator=(const UnixSocketTransport&) = delete;

    // Create the listening socket. Unlinks any pre-existing file at the path.
    // Returns false (with stderr message) on failure; agent should give up.
    bool start_listening();

    // Block until a client connects or shutdown() is called.
    // Returns true on connect (client_fd is held internally), false on shutdown.
    bool accept_blocking();

    // Write one framed packet to the current client. Returns false on
    // disconnect (caller should call disconnect_client() then accept_blocking()
    // again).
    bool write_packet(const std::vector<std::uint8_t>& packet);

    // Read exactly `len` bytes from the current client into `buf`,
    // looping over short reads. Returns true on success, false on
    // disconnect / shutdown / error. Designed for the input-reader
    // thread that runs in parallel with write_packet from the IO
    // thread (POSIX: concurrent recv/send on the same socket fd are
    // safe).
    bool read_exactly(std::uint8_t* buf, std::size_t len);

    // Close the current client connection. Idempotent.
    void disconnect_client();

    // Signal shutdown. Safe to call from any thread. Wakes accept_blocking()
    // and causes write_packet to fail. Idempotent.
    void shutdown();

private:
    bool write_all(int fd, const std::uint8_t* data, std::size_t len);

    std::string path_;
    int listen_fd_ = -1;
    int client_fd_ = -1;

    // Self-pipe used to wake the IO thread out of poll() when shutdown()
    // is called from another thread. shutdown_pipe_[0] is read end (poll'd),
    // shutdown_pipe_[1] is write end (written by shutdown()).
    int shutdown_pipe_[2] = {-1, -1};

    std::atomic<bool> shutting_down_{false};
};

}  // namespace agent
