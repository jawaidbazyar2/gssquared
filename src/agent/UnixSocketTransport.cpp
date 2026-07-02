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

#include "agent/UnixSocketTransport.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace agent {

namespace {

void set_cloexec(int fd) {
    int flags = ::fcntl(fd, F_GETFD, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}

void set_nonblock(int fd) {
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

}  // namespace

UnixSocketTransport::UnixSocketTransport(std::string path)
    : path_(std::move(path)) {}

UnixSocketTransport::~UnixSocketTransport() {
    shutdown();
    disconnect_client();
    if (listen_fd_ >= 0) {
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (shutdown_pipe_[0] >= 0) {
        ::close(shutdown_pipe_[0]);
        ::close(shutdown_pipe_[1]);
        shutdown_pipe_[0] = shutdown_pipe_[1] = -1;
    }
    // Best-effort: remove the socket file we created.
    if (!path_.empty()) {
        ::unlink(path_.c_str());
    }
}

bool UnixSocketTransport::start_listening() {
    if (path_.empty()) {
        std::fprintf(stderr, "[agent] socket path is empty\n");
        return false;
    }
    if (path_.size() >= sizeof(sockaddr_un{}.sun_path)) {
        std::fprintf(stderr, "[agent] socket path too long: %s\n", path_.c_str());
        return false;
    }

    // Self-pipe for shutdown wakeup. Read end is non-blocking so we can
    // drain it without risking a hang.
    if (::pipe(shutdown_pipe_) != 0) {
        std::fprintf(stderr, "[agent] pipe failed: %s\n", std::strerror(errno));
        return false;
    }
    set_cloexec(shutdown_pipe_[0]);
    set_cloexec(shutdown_pipe_[1]);
    set_nonblock(shutdown_pipe_[0]);

    listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
        std::fprintf(stderr, "[agent] socket() failed: %s\n", std::strerror(errno));
        return false;
    }
    set_cloexec(listen_fd_);

    // Remove any leftover socket file from a prior run.
    ::unlink(path_.c_str());

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "[agent] bind(%s) failed: %s\n",
                     path_.c_str(), std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (::listen(listen_fd_, 1) != 0) {
        std::fprintf(stderr, "[agent] listen() failed: %s\n", std::strerror(errno));
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    std::fprintf(stderr, "[agent] listening on %s\n", path_.c_str());
    return true;
}

bool UnixSocketTransport::accept_blocking() {
    if (shutting_down_.load(std::memory_order_acquire)) return false;
    if (listen_fd_ < 0) return false;

    pollfd pfds[2];
    pfds[0].fd = listen_fd_;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    pfds[1].fd = shutdown_pipe_[0];
    pfds[1].events = POLLIN;
    pfds[1].revents = 0;

    for (;;) {
        int n = ::poll(pfds, 2, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            std::fprintf(stderr, "[agent] poll() failed: %s\n", std::strerror(errno));
            return false;
        }
        if (pfds[1].revents & POLLIN) {
            // Shutdown signaled. Drain the pipe and bail.
            char buf[16];
            while (::read(shutdown_pipe_[0], buf, sizeof(buf)) > 0) {}
            return false;
        }
        if (pfds[0].revents & POLLIN) {
            int fd = ::accept(listen_fd_, nullptr, nullptr);
            if (fd < 0) {
                if (errno == EINTR || errno == EAGAIN) continue;
                std::fprintf(stderr, "[agent] accept() failed: %s\n",
                             std::strerror(errno));
                return false;
            }
            set_cloexec(fd);
            // Keep the client socket blocking — we want write() to apply
            // back-pressure rather than us tracking partial writes.
            client_fd_ = fd;
            std::fprintf(stderr, "[agent] compositor connected\n");
            return true;
        }
    }
}

bool UnixSocketTransport::write_packet(const std::vector<std::uint8_t>& packet) {
    if (client_fd_ < 0) return false;
    if (packet.empty()) return true;
    return write_all(client_fd_, packet.data(), packet.size());
}

bool UnixSocketTransport::read_exactly(std::uint8_t* buf, std::size_t len) {
    if (client_fd_ < 0) return false;
    std::size_t got = 0;
    while (got < len) {
        ssize_t r = ::recv(client_fd_, buf + got, len - got, 0);
        if (r == 0) {
            // EOF — peer closed.
            return false;
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            // EBADF / ECONNRESET / disconnect via shutdown(): treat as EOF.
            return false;
        }
        got += static_cast<std::size_t>(r);
    }
    return true;
}

bool UnixSocketTransport::write_all(int fd, const std::uint8_t* data, std::size_t len) {
    std::size_t sent = 0;
    while (sent < len) {
        // MSG_NOSIGNAL: don't raise SIGPIPE on disconnect; surface as EPIPE.
        // macOS uses SO_NOSIGPIPE on the socket instead, but MSG_NOSIGNAL
        // is portable enough on modern macOS / Linux for our purposes.
#ifdef MSG_NOSIGNAL
        const int flags = MSG_NOSIGNAL;
#else
        const int flags = 0;
#endif
        ssize_t r = ::send(fd, data + sent, len - sent, flags);
        if (r < 0) {
            if (errno == EINTR) continue;
            // EPIPE / ECONNRESET / EBADF — caller should disconnect_client.
            return false;
        }
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

void UnixSocketTransport::disconnect_client() {
    if (client_fd_ >= 0) {
        ::close(client_fd_);
        client_fd_ = -1;
        std::fprintf(stderr, "[agent] compositor disconnected\n");
    }
}

void UnixSocketTransport::shutdown() {
    if (shutting_down_.exchange(true, std::memory_order_acq_rel)) return;
    // Wake accept_blocking()'s poll().
    if (shutdown_pipe_[1] >= 0) {
        const char b = 'x';
        ssize_t ignored = ::write(shutdown_pipe_[1], &b, 1);
        (void)ignored;
    }
    // Force-break any in-progress send() in write_all(). Safe to call
    // concurrently with the IO thread's send.
    if (client_fd_ >= 0) {
        ::shutdown(client_fd_, SHUT_RDWR);
    }
}

}  // namespace agent
