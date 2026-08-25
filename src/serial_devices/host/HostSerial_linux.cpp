/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "serial_devices/host/HostSerial.hpp"

#ifndef HOSTSERIAL_LINUX_DEBUG
#define HOSTSERIAL_LINUX_DEBUG 0
#endif

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
    /* Linux USB-serial (esp. pl2303) can return EINVAL for O_NONBLOCK in open().
     * Open blocking like minicom, then switch to nonblocking. */
    int fd = open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (fd < 0) {
        fd = open(path, O_RDWR | O_NOCTTY);
    }
    if (fd < 0) {
        return -1;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    return fd;
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

speed_t posix_speed(uint32_t baud, bool *exact) {
    if (exact) {
        *exact = true;
    }
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
        default:
            if (exact) {
                *exact = false;
            }
            return B9600;
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

/** Match Windows: no RTS/CTS or XON/XOFF; raise DTR+RTS so USB-UART chips actually TX. */
void prepare_raw_cflags(struct termios &tio) {
    cfmakeraw(&tio);
    tio.c_cflag |= static_cast<tcflag_t>(CLOCAL | CREAD);
#ifdef CRTSCTS
    tio.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);
#endif
#ifdef HUPCL
    tio.c_cflag &= ~static_cast<tcflag_t>(HUPCL);
#endif
    tio.c_iflag &= ~static_cast<tcflag_t>(IXON | IXOFF | IXANY);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 0;
}

void assert_modem_lines(int fd) {
#ifdef TIOCMGET
    int bits = 0;
    if (ioctl(fd, TIOCMGET, &bits) == 0) {
        bits |= TIOCM_DTR | TIOCM_RTS;
        ioctl(fd, TIOCMSET, &bits);
    }
#else
    (void)fd;
#endif
}

#if HOSTSERIAL_LINUX_DEBUG

const char *speed_name(speed_t spd) {
    switch (spd) {
        case B0: return "0";
        case B50: return "50";
        case B75: return "75";
        case B110: return "110";
        case B134: return "134";
        case B150: return "150";
        case B200: return "200";
        case B300: return "300";
        case B600: return "600";
        case B1200: return "1200";
        case B1800: return "1800";
        case B2400: return "2400";
        case B4800: return "4800";
        case B9600: return "9600";
        case B19200: return "19200";
        case B38400: return "38400";
        case B57600: return "57600";
        case B115200: return "115200";
        case B230400: return "230400";
#ifdef B460800
        case B460800: return "460800";
#endif
        default: return "other";
    }
}

#define HS_ON(bits, bit, name) (((bits) & (bit)) ? " " name : " -" name)

void dump_port_state(int fd, const char *when, const host_serial_line_t *want) {
    struct termios tio {};
    const int tg = tcgetattr(fd, &tio);
    printf( "HostSerial: %s fd=%d tcgetattr=%s\n", when, fd,
            tg == 0 ? "ok" : strerror(errno));
    if (want) {
        printf( "HostSerial:   requested baud=%u data=%u stop=%u parity=%u\n",
                want->baud, want->data_bits, want->stop_bits, want->parity);
    }
    if (tg == 0) {
        const tcflag_t c = tio.c_cflag;
        const tcflag_t i = tio.c_iflag;
        printf(
                "HostSerial:   cflag=0x%lx%s%s%s%s%s%s%s%s  csize=%u\n",
                static_cast<unsigned long>(c),
                HS_ON(c, CLOCAL, "clocal"),
                HS_ON(c, CREAD, "cread"),
#ifdef CRTSCTS
                HS_ON(c, CRTSCTS, "crtscts"),
#else
                "",
#endif
#ifdef HUPCL
                HS_ON(c, HUPCL, "hupcl"),
#else
                "",
#endif
                HS_ON(c, PARENB, "parenb"),
                HS_ON(c, PARODD, "parodd"),
                HS_ON(c, CSTOPB, "cstopb"),
#ifdef CMSPAR
                HS_ON(c, CMSPAR, "cmspar"),
#else
                "",
#endif
                ((c & CSIZE) == CS5) ? 5u : ((c & CSIZE) == CS6) ? 6u :
                ((c & CSIZE) == CS7) ? 7u : 8u);
        printf( "HostSerial:   iflag=0x%lx%s%s%s  oflag=0x%lx lflag=0x%lx vmin=%u vtime=%u\n",
                static_cast<unsigned long>(i),
                HS_ON(i, IXON, "ixon"),
                HS_ON(i, IXOFF, "ixoff"),
                HS_ON(i, IXANY, "ixany"),
                static_cast<unsigned long>(tio.c_oflag),
                static_cast<unsigned long>(tio.c_lflag),
                tio.c_cc[VMIN], tio.c_cc[VTIME]);
        printf( "HostSerial:   posix ispeed=%s ospeed=%s\n",
                speed_name(cfgetispeed(&tio)), speed_name(cfgetospeed(&tio)));
    }

#ifdef TCGETS2
    struct termios2 tio2 {};
    if (ioctl(fd, TCGETS2, &tio2) == 0) {
        printf( "HostSerial:   termios2 ispeed=%u ospeed=%u cflag=0x%x%s\n",
                tio2.c_ispeed, tio2.c_ospeed, tio2.c_cflag,
#ifdef BOTHER
                (tio2.c_cflag & BOTHER) ? " bother" : " -bother"
#else
                ""
#endif
        );
    }
#endif

#ifdef TIOCMGET
    int m = 0;
    if (ioctl(fd, TIOCMGET, &m) == 0) {
        printf( "HostSerial:   modem=0x%x%s%s%s%s%s%s\n", m,
                HS_ON(m, TIOCM_DTR, "dtr"),
                HS_ON(m, TIOCM_RTS, "rts"),
                HS_ON(m, TIOCM_CTS, "cts"),
                HS_ON(m, TIOCM_DSR, "dsr"),
#ifdef TIOCM_CD
                HS_ON(m, TIOCM_CD, "cd"),
#else
                "",
#endif
                HS_ON(m, TIOCM_RNG, "ri"));
    } else {
        printf( "HostSerial:   TIOCMGET failed: %s\n", strerror(errno));
    }
#endif

    const int fl = fcntl(fd, F_GETFL);
    printf( "HostSerial:   fcntl=0x%x%s\n", fl,
            (fl >= 0 && (fl & O_NONBLOCK)) ? " nonblock" : " -nonblock");

    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = POLLIN | POLLOUT;
    const int pr = poll(&pfd, 1, 0);
    printf( "HostSerial:   poll=%d revents=0x%x%s%s%s%s%s\n", pr, pfd.revents,
            HS_ON(pfd.revents, POLLIN, "in"),
            HS_ON(pfd.revents, POLLOUT, "out"),
            HS_ON(pfd.revents, POLLERR, "err"),
            HS_ON(pfd.revents, POLLHUP, "hup"),
            HS_ON(pfd.revents, POLLNVAL, "nval"));
    fflush(stdout);
}

#undef HS_ON

void log_poll_detach(int fd, const char *op, int pr, short revents) {
    printf( "HostSerial: %s poll detach pr=%d revents=0x%x (err=%d hup=%d nval=%d in=%d out=%d)\n",
            op, pr, static_cast<unsigned>(revents),
            (revents & POLLERR) != 0, (revents & POLLHUP) != 0, (revents & POLLNVAL) != 0,
            (revents & POLLIN) != 0, (revents & POLLOUT) != 0);
    dump_port_state(fd, "on poll detach", nullptr);
}

bool logged_hup = false;

#define HS_LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)

#else

void dump_port_state(int, const char *, const host_serial_line_t *) {}
void log_poll_detach(int, const char *, int, short) {}
#define HS_LOG(...) ((void)0)

#endif

/** POLLHUP is normal on 3-wire null modems (no CD). Only ERR/NVAL are fatal. */
short poll_once(int fd, short events) {
    struct pollfd pfd {};
    pfd.fd = fd;
    pfd.events = events;
    const int pr = poll(&pfd, 1, 0);
    if (pr < 0) {
        return static_cast<short>(pr);
    }
#if HOSTSERIAL_LINUX_DEBUG
    if ((pfd.revents & POLLHUP) && !logged_hup) {
        HS_LOG("HostSerial: POLLHUP (no carrier); continuing with CLOCAL. revents=0x%x\n",
               static_cast<unsigned>(pfd.revents));
        logged_hup = true;
    }
#endif
    if (pfd.revents & (POLLERR | POLLNVAL)) {
        log_poll_detach(fd, (events & POLLOUT) ? "send" : "recv", pr, pfd.revents);
        errno = EIO;
        return -1;
    }
    if (pfd.revents & events) {
        return 1;
    }
    return 0;
}

bool apply_line(int fd, const host_serial_line_t &line) {
    struct termios tio {};
    if (tcgetattr(fd, &tio) != 0) {
        return false;
    }

    prepare_raw_cflags(tio);

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
    bool exact = true;
    const speed_t spd = posix_speed(baud, &exact);
    cfsetispeed(&tio, spd);
    cfsetospeed(&tio, spd);
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        return false;
    }

    /* BOTHER on standard rates breaks some Prolific/FTDI drivers (minicom uses B9600). */
    if (!exact) {
        apply_exact_baud(fd, baud);
    }
    assert_modem_lines(fd);
#if HOSTSERIAL_LINUX_DEBUG
    logged_hup = false;
#endif
    dump_port_state(fd, "after apply_line", &line);
    return true;
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
        HS_LOG("HostSerial: attach failed, empty path\n");
        return false;
    }

    std::string want = path;
    HS_LOG("HostSerial: opening %s\n", want.c_str());

    int fd = open_path(want.c_str());
    if (fd < 0 && want[0] != '/') {
        want = std::string("/dev/") + path;
        HS_LOG("HostSerial: retry %s\n", want.c_str());
        fd = open_path(want.c_str());
    }
    if (fd < 0) {
#if HOSTSERIAL_LINUX_DEBUG
        const int saved = errno;
        HS_LOG("HostSerial: open failed path=%s errno=%d %s\n", want.c_str(), saved,
               strerror(saved));
#endif
        return false;
    }

#ifdef TIOCEXCL
    ioctl(fd, TIOCEXCL);
#endif

    HS_LOG("HostSerial: attach path=%s fd=%d, applying defaults\n", path, fd);
    host_serial_line_t defaults;
    apply_line(fd, defaults);
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
    return apply_line(fd_, line);
}

int HostSerial::send(const uint8_t *data, int n) {
    if (fd_ < 0) {
        return -1;
    }
    if (n <= 0 || data == nullptr) {
        return 0;
    }

    const int pr = poll_once(fd_, POLLOUT);
    if (pr < 0) {
        if (errno == EINTR) {
            return 0;
        }
        HS_LOG("HostSerial: send poll errno=%s\n", strerror(errno));
        detach();
        return -1;
    }
    if (pr == 0) {
        return 0;
    }

    const ssize_t w = write(fd_, data, static_cast<size_t>(n));
    if (w < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        HS_LOG("HostSerial: write errno=%s\n", strerror(errno));
        dump_port_state(fd_, "on write error", nullptr);
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

    const int pr = poll_once(fd_, POLLIN);
    if (pr < 0) {
        if (errno == EINTR) {
            return 0;
        }
        HS_LOG("HostSerial: recv poll errno=%s\n", strerror(errno));
        detach();
        return -1;
    }
    if (pr == 0) {
        return 0;
    }

    const ssize_t r = read(fd_, data, static_cast<size_t>(n));
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return 0;
        }
        HS_LOG("HostSerial: read errno=%s\n", strerror(errno));
        dump_port_state(fd_, "on read error", nullptr);
        detach();
        return -1;
    }
    if (r == 0) {
        HS_LOG("HostSerial: read EOF (hangup)\n");
        dump_port_state(fd_, "on read EOF", nullptr);
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
