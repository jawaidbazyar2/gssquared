/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "serial_devices/host/HostSerial.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <IOKit/serial/ioss.h>

namespace {

int open_path(const char *path) {
    return open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
}

/** Skip always-present macOS system callouts, not USB/UART dongles. */
bool is_system_callout(const std::string &display) {
    std::string name = display;
    if (name.rfind("cu.", 0) == 0) {
        name.erase(0, 3);
    }
    for (char &c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (name.rfind("bluetooth", 0) == 0) {
        return true;
    }
    return name == "debug-console" || name == "wlan-debug";
}

speed_t nearest_posix_speed(uint32_t baud) {
    switch (baud) {
        case 50: return B50;
        case 75: return B75;
        case 110: return B110;
        case 134: return B134;
        case 150: return B150;
        case 200: return B200;
        case 300: return B300;
        case 600: return B600;
        case 1200: return B1200;
        case 1800: return B1800;
        case 2400: return B2400;
        case 4800: return B4800;
        case 9600: return B9600;
        case 19200: return B19200;
        case 38400: return B38400;
        case 57600: return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default: return B9600;
    }
}

} // namespace

HostSerial::~HostSerial() {
    detach();
}

bool HostSerial::attach(const char *path) {
    detach();
    if (path == nullptr || path[0] == '\0') {
        return false;
    }

    int fd = open_path(path);
    if (fd < 0 && path[0] != '/') {
        std::string fallback = std::string("/dev/") + path;
        fd = open_path(fallback.c_str());
    }
    if (fd < 0) {
        return false;
    }

#ifdef TIOCEXCL
    ioctl(fd, TIOCEXCL);
#endif

    struct termios tio {};
    if (tcgetattr(fd, &tio) == 0) {
        cfmakeraw(&tio);
        tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
        tio.c_cc[VMIN] = 0;
        tio.c_cc[VTIME] = 0;
        tcsetattr(fd, TCSANOW, &tio);
    }
    tcflush(fd, TCIOFLUSH);

    fd_ = fd;
    return true;
}

void HostSerial::detach() {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

bool HostSerial::configure(const host_serial_line_t &line) {
    if (fd_ < 0) {
        return false;
    }

    struct termios tio {};
    if (tcgetattr(fd_, &tio) != 0) {
        return false;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;

    tio.c_cflag &= ~static_cast<tcflag_t>(CSIZE);
    switch (line.data_bits) {
        case 5: tio.c_cflag |= CS5; break;
        case 6: tio.c_cflag |= CS6; break;
        case 7: tio.c_cflag |= CS7; break;
        default: tio.c_cflag |= CS8; break;
    }

    if (line.stop_bits == 2 || (line.stop_bits == 15 && line.data_bits == 5)) {
        tio.c_cflag |= CSTOPB;
    } else {
        tio.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);
    }

    tio.c_cflag &= ~static_cast<tcflag_t>(PARENB | PARODD);
    switch (line.parity) {
        case HOST_SERIAL_PARITY_EVEN:
            tio.c_cflag |= PARENB;
            break;
        case HOST_SERIAL_PARITY_ODD:
            tio.c_cflag |= static_cast<tcflag_t>(PARENB | PARODD);
            break;
        default:
            break;
    }

    const uint32_t baud = (line.baud == 0) ? 9600 : line.baud;
    const speed_t spd = nearest_posix_speed(baud);
    cfsetispeed(&tio, spd);
    cfsetospeed(&tio, spd);
    if (tcsetattr(fd_, TCSANOW, &tio) != 0) {
        return false;
    }

    speed_t exact = static_cast<speed_t>(baud);
    ioctl(fd_, IOSSIOSPEED, &exact);
    return true;
}

int HostSerial::send(const uint8_t *data, int n) {
    if (fd_ < 0) {
        return -1;
    }
    if (n <= 0 || data == nullptr) {
        return 0;
    }

    struct pollfd pfd {};
    pfd.fd = fd_;
    pfd.events = POLLOUT;
    const int pr = poll(&pfd, 1, 0);
    if (pr < 0) {
        if (errno == EINTR) {
            return 0;
        }
        detach();
        return -1;
    }
    if (pr == 0) {
        return 0;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        detach();
        return -1;
    }

    const ssize_t w = write(fd_, data, static_cast<size_t>(n));
    if (w < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        detach();
        return -1;
    }
    return static_cast<int>(w);
}

int HostSerial::receive(uint8_t *data, int n) {
    if (fd_ < 0) {
        return -1;
    }
    if (n <= 0 || data == nullptr) {
        return 0;
    }

    struct pollfd pfd {};
    pfd.fd = fd_;
    pfd.events = POLLIN;
    const int pr = poll(&pfd, 1, 0);
    if (pr < 0) {
        if (errno == EINTR) {
            return 0;
        }
        detach();
        return -1;
    }
    if (pr == 0) {
        return 0;
    }
    if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        detach();
        return -1;
    }

    const ssize_t r = read(fd_, data, static_cast<size_t>(n));
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        detach();
        return -1;
    }
    if (r == 0) {
        // USB-serial hangup often surfaces as EOF
        detach();
        return -1;
    }
    return static_cast<int>(r);
}

std::vector<host_serial_info_t> host_serial_enumerate() {
    std::vector<host_serial_info_t> out;
    glob_t g {};
    const int rc = glob("/dev/cu.*", GLOB_NOSORT, nullptr, &g);
    if (rc == 0) {
        for (size_t i = 0; i < g.gl_pathc; i++) {
            host_serial_info_t info;
            info.path = g.gl_pathv[i];
            const char *slash = std::strrchr(g.gl_pathv[i], '/');
            info.display = slash ? slash + 1 : info.path;
            if (is_system_callout(info.display)) {
                continue;
            }
            out.push_back(std::move(info));
        }
        globfree(&g);
    }
    std::sort(out.begin(), out.end(), [](const host_serial_info_t &a, const host_serial_info_t &b) {
        return a.display < b.display;
    });
    return out;
}
