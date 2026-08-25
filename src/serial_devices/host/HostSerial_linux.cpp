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
#include <cstdlib>
#include <cstring>
#include <limits.h>
#include <set>
#include <string>
#include <vector>

#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace {

int open_path(const char *path) {
    return open(path, O_RDWR | O_NOCTTY | O_NONBLOCK);
}

/** Skip Bluetooth and debug nodes that clutter the picker. */
bool is_skipped_name(const std::string &display) {
    std::string name = display;
    for (char &c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name.find("bluetooth") != std::string::npos ||
           name.find("rfcomm") != std::string::npos ||
           name.find("debug") != std::string::npos;
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
#ifdef B460800
        case 460800: return B460800;
#endif
#ifdef B500000
        case 500000: return B500000;
#endif
#ifdef B576000
        case 576000: return B576000;
#endif
#ifdef B921600
        case 921600: return B921600;
#endif
#ifdef B1000000
        case 1000000: return B1000000;
#endif
#ifdef B1152000
        case 1152000: return B1152000;
#endif
#ifdef B1500000
        case 1500000: return B1500000;
#endif
#ifdef B2000000
        case 2000000: return B2000000;
#endif
#ifdef B2500000
        case 2500000: return B2500000;
#endif
#ifdef B3000000
        case 3000000: return B3000000;
#endif
#ifdef B3500000
        case 3500000: return B3500000;
#endif
#ifdef B4000000
        case 4000000: return B4000000;
#endif
        default: return B9600;
    }
}

/*
 * Kernel termios2 ABI (NCCS=19). Declared here instead of including
 * <asm/termbits.h>, which redefines glibc's struct termios.
 */
struct termios2 {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_line;
    unsigned char c_cc[19];
    unsigned int c_ispeed;
    unsigned int c_ospeed;
};

#ifndef BOTHER
#define BOTHER 0x00001000
#endif

void apply_exact_baud(int fd, uint32_t baud) {
#ifdef TCGETS2
    struct termios2 tio2 {};
    if (ioctl(fd, TCGETS2, &tio2) != 0) {
        return;
    }
#ifdef CBAUD
    tio2.c_cflag &= ~static_cast<unsigned int>(CBAUD);
#endif
    tio2.c_cflag |= BOTHER;
    tio2.c_ispeed = baud;
    tio2.c_ospeed = baud;
    ioctl(fd, TCSETS2, &tio2);
#else
    (void)fd;
    (void)baud;
#endif
}

void glob_paths(const char *pattern, std::vector<std::string> &out) {
    glob_t g {};
    if (glob(pattern, GLOB_NOSORT, nullptr, &g) != 0) {
        return;
    }
    for (size_t i = 0; i < g.gl_pathc; i++) {
        out.emplace_back(g.gl_pathv[i]);
    }
    globfree(&g);
}

/** Kernel node (/dev/ttyUSB0), not a long udev by-id symlink. */
std::string canonical_port_path(const std::string &path) {
    char real[PATH_MAX];
    if (realpath(path.c_str(), real) != nullptr) {
        return real;
    }
    return path;
}

void add_port(std::vector<host_serial_info_t> &out, const std::string &path,
              std::set<std::string> &seen_real) {
    const std::string stored = canonical_port_path(path);
    const char *slash = std::strrchr(stored.c_str(), '/');
    std::string display = slash ? slash + 1 : stored;
    if (is_skipped_name(display)) {
        return;
    }
    if (!seen_real.insert(stored).second) {
        return;
    }

    host_serial_info_t info;
    info.path = stored;
    info.display = std::move(display);
    out.push_back(std::move(info));
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
        if (fd < 0) {
            const std::string real = canonical_port_path(fallback);
            if (real != fallback) {
                fd = open_path(real.c_str());
            }
        }
    }
    if (fd < 0) {
        const std::string real = canonical_port_path(path);
        if (real != path) {
            fd = open_path(real.c_str());
        }
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
#ifdef CRTSCTS
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
#endif
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

    apply_exact_baud(fd_, baud);
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
    std::set<std::string> seen_real;

    std::vector<std::string> paths;
    glob_paths("/dev/ttyUSB*", paths);
    glob_paths("/dev/ttyACM*", paths);
    glob_paths("/dev/ttyAMA*", paths);

    for (const std::string &path : paths) {
        add_port(out, path, seen_real);
    }

    std::sort(out.begin(), out.end(), [](const host_serial_info_t &a, const host_serial_info_t &b) {
        return a.display < b.display;
    });
    return out;
}
