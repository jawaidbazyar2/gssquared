/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "serial_devices/host/HostSerial.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

HANDLE as_handle(void *h) {
    return static_cast<HANDLE>(h);
}

/** COM3 / com3: / \\.\COM10 → \\.\COMn (required for COM10+). */
std::string win32_comm_path(const char *path) {
    std::string p = path;
    if (p.size() >= 4 && p.compare(0, 4, "\\\\.\\") == 0) {
        return p;
    }
    if (!p.empty() && p.back() == ':') {
        p.pop_back();
    }
    return std::string("\\\\.\\") + p;
}

bool is_bluetooth_device(const char *nt_name) {
    std::string name = nt_name;
    for (char &c : name) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return name.find("bthmodem") != std::string::npos ||
           name.find("bluetooth") != std::string::npos;
}

int comm_number(const std::string &path) {
    const auto pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    if (name.size() < 4) {
        return -1;
    }
    if (std::tolower(static_cast<unsigned char>(name[0])) != 'c' ||
        std::tolower(static_cast<unsigned char>(name[1])) != 'o' ||
        std::tolower(static_cast<unsigned char>(name[2])) != 'm' ||
        !std::isdigit(static_cast<unsigned char>(name[3]))) {
        return -1;
    }
    return std::atoi(name.c_str() + 3);
}

bool is_timeout_error(DWORD err) {
    return err == ERROR_TIMEOUT || err == ERROR_COUNTER_TIMEOUT || err == WAIT_TIMEOUT;
}

bool is_hard_error(DWORD err) {
    switch (err) {
        case ERROR_ACCESS_DENIED:
        case ERROR_BAD_COMMAND:
        case ERROR_OPERATION_ABORTED:
        case ERROR_INVALID_HANDLE:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_GEN_FAILURE:
        case ERROR_DEVICE_NOT_CONNECTED:
        case ERROR_BAD_DEVICE:
            return true;
        default:
            return false;
    }
}

bool apply_timeouts(HANDLE h) {
    COMMTIMEOUTS t {};
    t.ReadIntervalTimeout = MAXDWORD;
    t.ReadTotalTimeoutMultiplier = 0;
    t.ReadTotalTimeoutConstant = 0;
    t.WriteTotalTimeoutMultiplier = 0;
    t.WriteTotalTimeoutConstant = 1;
    return SetCommTimeouts(h, &t) != 0;
}

void fill_line_dcb(DCB &dcb, const host_serial_line_t &line) {
    dcb.fBinary = TRUE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;

    dcb.BaudRate = (line.baud == 0) ? 9600 : line.baud;

    switch (line.data_bits) {
        case 5: dcb.ByteSize = 5; break;
        case 6: dcb.ByteSize = 6; break;
        case 7: dcb.ByteSize = 7; break;
        default: dcb.ByteSize = 8; break;
    }

    if (line.stop_bits == 15) {
        dcb.StopBits = ONE5STOPBITS;
    } else if (line.stop_bits == 2) {
        dcb.StopBits = TWOSTOPBITS;
    } else {
        dcb.StopBits = ONESTOPBIT;
    }

    switch (line.parity) {
        case HOST_SERIAL_PARITY_EVEN:
            dcb.Parity = EVENPARITY;
            dcb.fParity = TRUE;
            break;
        case HOST_SERIAL_PARITY_ODD:
            dcb.Parity = ODDPARITY;
            dcb.fParity = TRUE;
            break;
        case HOST_SERIAL_PARITY_MARK:
            dcb.Parity = MARKPARITY;
            dcb.fParity = TRUE;
            break;
        case HOST_SERIAL_PARITY_SPACE:
            dcb.Parity = SPACEPARITY;
            dcb.fParity = TRUE;
            break;
        default:
            dcb.Parity = NOPARITY;
            dcb.fParity = FALSE;
            break;
    }
}

bool line_already_set(const DCB &dcb, const host_serial_line_t &line) {
    DCB want = dcb;
    fill_line_dcb(want, line);
    return dcb.BaudRate == want.BaudRate && dcb.ByteSize == want.ByteSize &&
           dcb.StopBits == want.StopBits && dcb.Parity == want.Parity &&
           dcb.fParity == want.fParity && dcb.fBinary == want.fBinary &&
           dcb.fOutxCtsFlow == want.fOutxCtsFlow && dcb.fOutxDsrFlow == want.fOutxDsrFlow &&
           dcb.fDtrControl == want.fDtrControl && dcb.fRtsControl == want.fRtsControl &&
           dcb.fOutX == want.fOutX && dcb.fInX == want.fInX &&
           dcb.fAbortOnError == want.fAbortOnError;
}

bool apply_line(HANDLE h, const host_serial_line_t &line) {
    DCB dcb {};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb)) {
        return false;
    }

    /* SetCommState often resets COMMTIMEOUTS and can flush USB-serial RX. */
    if (line_already_set(dcb, line)) {
        return true;
    }

    fill_line_dcb(dcb, line);
    if (!SetCommState(h, &dcb)) {
        return false;
    }
    return apply_timeouts(h);
}

bool comm_error_is_fatal(HANDLE h, DWORD err) {
    if (is_timeout_error(err)) {
        return false;
    }
    DWORD errors = 0;
    COMSTAT st {};
    if (!ClearCommError(h, &errors, &st)) {
        return true;
    }
    return is_hard_error(err);
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

    const std::string comm = win32_comm_path(path);
    HANDLE h = CreateFileA(comm.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }

    SetupComm(h, 8192, 8192);

    if (!apply_timeouts(h)) {
        CloseHandle(h);
        return false;
    }

    host_serial_line_t defaults;
    apply_line(h, defaults);
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);

    handle_ = h;
    return true;
}

void HostSerial::detach() {
    if (handle_ != nullptr) {
        CloseHandle(as_handle(handle_));
        handle_ = nullptr;
    }
}

bool HostSerial::configure(const host_serial_line_t &line) {
    if (handle_ == nullptr) {
        return false;
    }
    return apply_line(as_handle(handle_), line);
}

int HostSerial::send(const uint8_t *data, int n) {
    if (handle_ == nullptr) {
        return -1;
    }
    if (n <= 0 || data == nullptr) {
        return 0;
    }

    HANDLE h = as_handle(handle_);
    DWORD written = 0;
    if (!WriteFile(h, data, static_cast<DWORD>(n), &written, nullptr)) {
        const DWORD err = GetLastError();
        if (is_timeout_error(err)) {
            return 0;
        }
        if (comm_error_is_fatal(h, err)) {
            detach();
            return -1;
        }
        return 0;
    }
    return static_cast<int>(written);
}

int HostSerial::receive(uint8_t *data, int n) {
    if (handle_ == nullptr) {
        return -1;
    }
    if (n <= 0 || data == nullptr) {
        return 0;
    }

    HANDLE h = as_handle(handle_);
    int total = 0;
    while (total < n) {
        DWORD errors = 0;
        COMSTAT st {};
        if (!ClearCommError(h, &errors, &st)) {
            detach();
            return -1;
        }
        if (st.cbInQue == 0) {
            break;
        }

        const DWORD want = static_cast<DWORD>(n - total) < st.cbInQue
                               ? static_cast<DWORD>(n - total)
                               : st.cbInQue;
        DWORD got = 0;
        if (!ReadFile(h, data + total, want, &got, nullptr)) {
            const DWORD err = GetLastError();
            if (is_timeout_error(err)) {
                break;
            }
            if (comm_error_is_fatal(h, err)) {
                detach();
                return -1;
            }
            break;
        }
        if (got == 0) {
            break;
        }
        total += static_cast<int>(got);
    }
    return total;
}

std::vector<host_serial_info_t> host_serial_enumerate() {
    std::vector<host_serial_info_t> out;
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "HARDWARE\\DEVICEMAP\\SERIALCOMM",
                      0,
                      KEY_READ,
                      &key) != ERROR_SUCCESS) {
        return out;
    }

    for (DWORD i = 0;; i++) {
        char nt_name[256];
        BYTE data[256];
        DWORD name_len = sizeof(nt_name);
        DWORD data_len = sizeof(data);
        DWORD type = 0;
        const LONG rc = RegEnumValueA(key, i, nt_name, &name_len, nullptr, &type, data, &data_len);
        if (rc == ERROR_NO_MORE_ITEMS) {
            break;
        }
        if (rc != ERROR_SUCCESS || type != REG_SZ || data_len == 0) {
            continue;
        }
        if (data_len >= sizeof(data)) {
            data_len = sizeof(data) - 1;
        }
        data[data_len] = 0;
        if (data[0] == '\0') {
            continue;
        }
        if (is_bluetooth_device(nt_name)) {
            continue;
        }

        host_serial_info_t info;
        info.path = reinterpret_cast<const char *>(data);
        info.display = info.path;
        out.push_back(std::move(info));
    }
    RegCloseKey(key);

    std::sort(out.begin(), out.end(), [](const host_serial_info_t &a, const host_serial_info_t &b) {
        const int na = comm_number(a.path);
        const int nb = comm_number(b.path);
        if (na >= 0 && nb >= 0 && na != nb) {
            return na < nb;
        }
        return a.display < b.display;
    });
    return out;
}
