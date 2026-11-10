/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
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

#include "serial_devices/print/Gs2PrintSession.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#if defined(__EMSCRIPTEN__)

struct Gs2PrintSession::Impl {};

Gs2PrintSession::Gs2PrintSession() : impl_(std::make_unique<Impl>()) {}
Gs2PrintSession::~Gs2PrintSession() = default;
void Gs2PrintSession::begin() {}
void Gs2PrintSession::write(uint8_t) {}
void Gs2PrintSession::poll() {}
void Gs2PrintSession::end() {}
bool Gs2PrintSession::take_app_missing() { return false; }
bool Gs2PrintSession::take_peer_closed() { return false; }

#else

#include <cerrno>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <afunix.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <filesystem>
#endif

namespace {

constexpr uint64_t kConnectTimeoutMs = 5000;

#if defined(_WIN32)
using sock_t = SOCKET;
constexpr sock_t kInvalidSock = INVALID_SOCKET;
#else
using sock_t = int;
constexpr sock_t kInvalidSock = -1;
#endif

std::atomic<uint32_t> &job_counter() {
    static std::atomic<uint32_t> n{0};
    return n;
}

void close_sock(sock_t &fd) {
    if (fd == kInvalidSock) {
        return;
    }
#if defined(_WIN32)
    ::closesocket(fd);
#else
    ::close(fd);
#endif
    fd = kInvalidSock;
}

void set_cloexec(sock_t fd) {
#if defined(_WIN32)
    SetHandleInformation(reinterpret_cast<HANDLE>(fd), HANDLE_FLAG_INHERIT, 0);
#else
    int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
#endif
}

void set_nonblock(sock_t fd) {
#if defined(_WIN32)
    u_long mode = 1;
    ::ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void set_nosigpipe(sock_t fd) {
#if defined(SO_NOSIGPIPE)
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}

bool ready_read(sock_t fd) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{};
    const int rc = ::select(static_cast<int>(fd) + 1, &rfds, nullptr, nullptr, &tv);
    return rc > 0 && FD_ISSET(fd, &rfds);
}

#if defined(_WIN32)
std::string wide_to_utf8(const wchar_t *w) {
    if (!w || !w[0]) {
        return {};
    }
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    std::string s(static_cast<size_t>(n - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring utf8_to_wide(const std::string &s) {
    if (s.empty()) {
        return {};
    }
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (n <= 1) {
        return {};
    }
    std::wstring w(static_cast<size_t>(n - 1), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

bool ensure_winsock() {
    static bool started = false;
    if (started) {
        return true;
    }
    WSADATA wsa{};
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    started = true;
    return true;
}
#endif

std::string temp_dir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH + 1]{};
    const DWORD n = ::GetTempPathW(MAX_PATH, buf);
    if (n == 0 || n > MAX_PATH) {
        return {};
    }
    return wide_to_utf8(buf);
#else
    std::error_code ec;
    const auto p = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return {};
    }
    return p.string();
#endif
}

std::string join_temp(const std::string &dir, const std::string &name) {
    if (dir.empty()) {
        return name;
    }
    const char last = dir.back();
    if (last == '/' || last == '\\') {
        return dir + name;
    }
#if defined(_WIN32)
    return dir + '\\' + name;
#else
    return dir + '/' + name;
#endif
}

void unlink_path(std::string &path) {
    if (path.empty()) {
        return;
    }
    std::remove(path.c_str());
    path.clear();
}

} // namespace

struct Gs2PrintSession::Impl {
    sock_t listen_fd = kInvalidSock;
    sock_t client_fd = kInvalidSock;
    SDL_Process *open_proc = nullptr;
    std::string socket_path;
    std::string ticket_path;
    std::vector<uint8_t> outbound;
    size_t out_off = 0;
    uint64_t start_ticks = 0;
    bool active = false;
    bool waiting = false;
    bool app_missing = false;
    bool missing_signaled = false;
    bool peer_closed = false;

    ~Impl() {
        dispose_process(true);
        end();
    }

    void begin() {
        dispose_process(true);
        end();
        missing_signaled = false;

        const std::string dir = temp_dir();
        const uint32_t seq = ++job_counter();
#if defined(_WIN32)
        if (!ensure_winsock()) {
            SDL_Log("Gs2Print: WSAStartup failed");
            give_up();
            active = true;
            return;
        }
        const unsigned long pid = ::GetCurrentProcessId();
#else
        const long pid = static_cast<long>(::getpid());
#endif
        char sock_name[64];
        char ticket_name[64];
        std::snprintf(sock_name, sizeof(sock_name), "gs2-print-%lu-%u.sock",
                      static_cast<unsigned long>(pid), seq);
        std::snprintf(ticket_name, sizeof(ticket_name), "gs2-print-%lu-%u.gs2print",
                      static_cast<unsigned long>(pid), seq);
        socket_path = join_temp(dir, sock_name);
        ticket_path = join_temp(dir, ticket_name);
        active = true;
        start_ticks = SDL_GetTicks();

        if (!listen_on(socket_path)) {
            give_up();
            return;
        }
        if (!write_ticket()) {
            give_up();
            return;
        }
        waiting = true;
        if (!spawn_open()) {
            give_up();
        }
    }

    void write(uint8_t byte) {
        if (client_fd == kInvalidSock) {
            if (waiting) {
                outbound.push_back(byte);
            }
            return;
        }
        outbound.push_back(byte);
        flush();
    }

    void poll() {
        reap_open();
        if (!active) {
            return;
        }
        if (waiting && client_fd == kInvalidSock) {
            if (SDL_GetTicks() - start_ticks >= kConnectTimeoutMs) {
                SDL_Log("Gs2Print: nothing connected within %u ms",
                        static_cast<unsigned>(kConnectTimeoutMs));
                give_up();
            } else if (listen_fd != kInvalidSock && ready_read(listen_fd)) {
                accept_one();
            }
        }
        if (client_fd != kInvalidSock) {
            drain_inbound();
            flush();
        }
    }

    void end() {
        close_sock(client_fd);
        close_sock(listen_fd);
        unlink_path(socket_path);
        unlink_path(ticket_path);
        outbound.clear();
        out_off = 0;
        waiting = false;
        active = false;
    }

    bool take_app_missing() {
        if (!app_missing) {
            return false;
        }
        app_missing = false;
        return true;
    }

    bool take_peer_closed() {
        if (!peer_closed) {
            return false;
        }
        peer_closed = false;
        return true;
    }

private:
    bool listen_on(const std::string &path) {
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (path.size() >= sizeof(addr.sun_path)) {
            SDL_Log("Gs2Print: socket path too long: %s", path.c_str());
            socket_path.clear();
            return false;
        }

        listen_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd == kInvalidSock) {
            SDL_Log("Gs2Print: socket() failed");
            return false;
        }
        set_cloexec(listen_fd);
        set_nonblock(listen_fd);

        std::remove(path.c_str());
        std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path.c_str());
#if defined(_WIN32)
        const int rc = ::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        if (rc == SOCKET_ERROR || ::listen(listen_fd, 1) == SOCKET_ERROR) {
            SDL_Log("Gs2Print: bind/listen(%s) failed: %d", path.c_str(), ::WSAGetLastError());
            close_sock(listen_fd);
            return false;
        }
#else
        if (::bind(listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0 ||
            ::listen(listen_fd, 1) < 0) {
            SDL_Log("Gs2Print: bind/listen(%s) failed: %s", path.c_str(), std::strerror(errno));
            close_sock(listen_fd);
            return false;
        }
#endif
        SDL_Log("Gs2Print: listening on %s", path.c_str());
        return true;
    }

    bool write_ticket() {
        FILE *f = std::fopen(ticket_path.c_str(), "wb");
        if (!f) {
            SDL_Log("Gs2Print: failed to write ticket %s", ticket_path.c_str());
            return false;
        }
        std::fprintf(f, "GS2PRINT 1\nprinter imagewriter-ii\nsocket %s\n\n", socket_path.c_str());
        std::fclose(f);
        SDL_Log("Gs2Print: ticket %s", ticket_path.c_str());
        return true;
    }

    bool spawn_open() {
#if defined(_WIN32)
        const std::wstring ticket_w = utf8_to_wide(ticket_path);
        if (ticket_w.empty()) {
            SDL_Log("Gs2Print: ticket path is empty");
            return false;
        }
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_ASYNCOK;
        info.lpVerb = L"open";
        info.lpFile = ticket_w.c_str();
        info.nShow = SW_SHOWDEFAULT;
        if (!::ShellExecuteExW(&info)) {
            SDL_Log("Gs2Print: ShellExecuteEx failed: %lu", ::GetLastError());
            return false;
        }
        return true;
#else
#if defined(__APPLE__)
        const char *args[] = {"/usr/bin/open", "--", ticket_path.c_str(), nullptr};
#else
        const char *args[] = {"xdg-open", ticket_path.c_str(), nullptr};
#endif
        open_proc = SDL_CreateProcess(args, false);
        if (!open_proc) {
            SDL_Log("Gs2Print: failed to spawn open: %s", SDL_GetError());
            return false;
        }
        return true;
#endif
    }

    void accept_one() {
#if defined(_WIN32)
        const SOCKET fd = ::accept(listen_fd, nullptr, nullptr);
        if (fd == INVALID_SOCKET) {
            return;
        }
#else
        const int fd = ::accept(listen_fd, nullptr, nullptr);
        if (fd < 0) {
            return;
        }
#endif
        set_cloexec(fd);
        set_nonblock(fd);
        set_nosigpipe(fd);
        client_fd = fd;
        waiting = false;
        close_sock(listen_fd);
        SDL_Log("Gs2Print: sidecar connected");
        flush();
    }

    void flush() {
        while (client_fd != kInvalidSock && out_off < outbound.size()) {
            const char *p = reinterpret_cast<const char *>(outbound.data() + out_off);
            const size_t left = outbound.size() - out_off;
#if defined(_WIN32)
            const int n = ::send(client_fd, p, static_cast<int>(left), 0);
            if (n == SOCKET_ERROR) {
                const int err = ::WSAGetLastError();
                if (err == WSAEWOULDBLOCK) {
                    return;
                }
                note_peer_closed();
                return;
            }
#else
            const ssize_t n = ::send(client_fd, p, left,
#if defined(MSG_NOSIGNAL)
                                      MSG_NOSIGNAL
#else
                                      0
#endif
            );
            if (n < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return;
                }
                note_peer_closed();
                return;
            }
#endif
            if (n == 0) {
                note_peer_closed();
                return;
            }
            out_off += static_cast<size_t>(n);
        }
        if (out_off > 0 && out_off == outbound.size()) {
            outbound.clear();
            out_off = 0;
        }
    }

    void drain_inbound() {
        if (client_fd == kInvalidSock || !ready_read(client_fd)) {
            return;
        }
        char buf[256];
#if defined(_WIN32)
        const int n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n == 0 || (n == SOCKET_ERROR && ::WSAGetLastError() != WSAEWOULDBLOCK)) {
            note_peer_closed();
        }
#else
        const ssize_t n = ::recv(client_fd, buf, sizeof(buf), 0);
        if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            note_peer_closed();
        }
#endif
    }

    void note_peer_closed() {
        if (peer_closed) {
            close_sock(client_fd);
            return;
        }
        peer_closed = true;
        close_sock(client_fd);
        outbound.clear();
        out_off = 0;
        SDL_Log("Gs2Print: sidecar closed the socket");
    }

    void give_up() {
        close_sock(listen_fd);
        unlink_path(socket_path);
        outbound.clear();
        out_off = 0;
        waiting = false;
        if (!missing_signaled) {
            missing_signaled = true;
            app_missing = true;
        }
    }

    void reap_open() {
        dispose_process(false);
    }

    void dispose_process(bool force_finish) {
        if (!open_proc) {
            return;
        }
        int code = 0;
        bool done = SDL_WaitProcess(open_proc, false, &code);
        if (!done && force_finish) {
            SDL_KillProcess(open_proc, true);
            SDL_WaitProcess(open_proc, true, &code);
            done = true;
            code = 0;
        }
        if (!done) {
            return;
        }
        SDL_DestroyProcess(open_proc);
        open_proc = nullptr;
        if (!force_finish && code != 0 && waiting && client_fd == kInvalidSock) {
            SDL_Log("Gs2Print: open exited %d", code);
            give_up();
        }
    }
};

Gs2PrintSession::Gs2PrintSession() : impl_(std::make_unique<Impl>()) {}
Gs2PrintSession::~Gs2PrintSession() = default;

void Gs2PrintSession::begin() { impl_->begin(); }
void Gs2PrintSession::write(uint8_t byte) { impl_->write(byte); }
void Gs2PrintSession::poll() { impl_->poll(); }
void Gs2PrintSession::end() { impl_->end(); }
bool Gs2PrintSession::take_app_missing() { return impl_->take_app_missing(); }
bool Gs2PrintSession::take_peer_closed() { return impl_->take_peer_closed(); }

#endif
