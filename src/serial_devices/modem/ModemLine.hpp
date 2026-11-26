#pragma once

#include <cstdint>
#include <memory>
#include <string>

/*
 * One modem connection, whatever carries it. ModemDevice uses it only on its
 * worker thread. A line passes bytes through unchanged.
 */
class ModemLine {
    public:
        virtual ~ModemLine() = default;
        /* Bytes read; 0 when nothing has arrived; -1 when the connection is closed. */
        virtual int read(uint8_t *buf, int len) = 0;
        /* false when the connection is lost. */
        virtual bool write(const uint8_t *buf, int len) = 0;
        /* Remote end, for the HUD. */
        virtual const std::string &peer() const = 0;
};

/* Inbound telnet callers. */
class ModemListener {
    public:
        virtual ~ModemListener() = default;
        /* A new caller, or nullptr. */
        virtual std::unique_ptr<ModemLine> accept() = 0;
};

/*
 * Each build links one implementation of these:
 *   ModemLine_net.cpp: telnet over TCP (SDL_net). No WebSocket.
 *   ModemLine_web.cpp: telnet over WebSocket (the browser). No TCP.
 * A dial blocks the worker thread until it connects or fails; nullptr means
 * no carrier. modem_listen_tcp() returns nullptr when it cannot listen.
 */
std::unique_ptr<ModemLine> modem_dial_tcp(const std::string &host, int port);
std::unique_ptr<ModemLine> modem_dial_websocket(const std::string &url);
std::unique_ptr<ModemListener> modem_listen_tcp(uint16_t port);
