#include "serial_devices/modem/ModemLine.hpp"

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>
#include <cstdio>
#include <utility>

namespace {

/* NET_Init and NET_Quit count their calls, so each object holds one. */
class TcpLine : public ModemLine {
    public:
        TcpLine(NET_StreamSocket *socket, std::string peer)
            : socket_(socket), peer_(std::move(peer)) {}
        ~TcpLine() override {
            NET_DestroyStreamSocket(socket_);
            NET_Quit();
        }
        int read(uint8_t *buf, int len) override {
            return NET_ReadFromStreamSocket(socket_, buf, len);
        }
        bool write(const uint8_t *buf, int len) override {
            if (!NET_WriteToStreamSocket(socket_, buf, len)) {
                printf("ModemDevice: Failed to send data: %s\n", SDL_GetError());
                return false;
            }
            return true;
        }
        const std::string &peer() const override { return peer_; }

    private:
        NET_StreamSocket *socket_;
        std::string peer_;
};

class TcpListener : public ModemListener {
    public:
        TcpListener(NET_Server *server, uint16_t port) : server_(server), port_(port) {}
        ~TcpListener() override {
            NET_DestroyServer(server_);
            NET_Quit();
        }
        std::unique_ptr<ModemLine> accept() override {
            NET_StreamSocket *incoming = nullptr;
            if (!NET_AcceptClient(server_, &incoming)) {
                printf("ModemDevice: AcceptClient failed: %s\n", SDL_GetError());
                return nullptr;
            }
            if (!incoming) {
                return nullptr;
            }
            std::string peer = "inbound";
            NET_Address *addr = NET_GetStreamSocketAddress(incoming);
            if (addr) {
                const char *str = NET_GetAddressString(addr);
                if (str && str[0] != '\0') {
                    peer = str;
                }
                NET_UnrefAddress(addr);
            }
            peer += ":" + std::to_string(port_);
            NET_Init();
            return std::make_unique<TcpLine>(incoming, peer);
        }

    private:
        NET_Server *server_;
        uint16_t port_;
};

} // namespace

std::unique_ptr<ModemLine> modem_dial_tcp(const std::string &host, int port) {
    if (!NET_Init()) {
        printf("ModemDevice: Failed to initialize SDL_net: %s\n", SDL_GetError());
        return nullptr;
    }
    printf("ModemDevice: Connecting to %s:%d\n", host.c_str(), port);

    // Resolve hostname (asynchronous)
    NET_Address *addr = NET_ResolveHostname(host.c_str());
    if (!addr) {
        printf("ModemDevice: Failed to start hostname resolution: %s\n", SDL_GetError());
        NET_Quit();
        return nullptr;
    }

    // Wait for resolution to complete (10 second timeout)
    NET_Status resolve_status = NET_WaitUntilResolved(addr, 10000);
    if (resolve_status != NET_SUCCESS) {
        printf("ModemDevice: Failed to resolve hostname: %s\n", SDL_GetError());
        NET_UnrefAddress(addr);
        NET_Quit();
        return nullptr;
    }

    // Create client socket (starts connection asynchronously)
    NET_StreamSocket *socket = NET_CreateClient(addr, port);
    NET_UnrefAddress(addr);

    if (!socket) {
        printf("ModemDevice: Failed to create client: %s\n", SDL_GetError());
        NET_Quit();
        return nullptr;
    }

    // Wait for connection to complete (30 second timeout)
    NET_Status connect_status = NET_WaitUntilConnected(socket, 30000);
    if (connect_status != NET_SUCCESS) {
        printf("ModemDevice: Failed to connect: %s\n", SDL_GetError());
        NET_DestroyStreamSocket(socket);
        NET_Quit();
        return nullptr;
    }
    return std::make_unique<TcpLine>(socket, host + ":" + std::to_string(port));
}

std::unique_ptr<ModemLine> modem_dial_websocket(const std::string &url) {
    printf("ModemDevice: %s: WebSocket dialing is only in the browser build\n", url.c_str());
    return nullptr;
}

std::unique_ptr<ModemListener> modem_listen_tcp(uint16_t port) {
    if (!NET_Init()) {
        printf("ModemDevice: Failed to initialize SDL_net: %s\n", SDL_GetError());
        return nullptr;
    }
    NET_Server *server = NET_CreateServer(nullptr, port);
    if (!server) {
        printf("ModemDevice: listen %u failed: %s (outbound only)\n",
               static_cast<unsigned>(port), SDL_GetError());
        NET_Quit();
        return nullptr;
    }
    printf("ModemDevice: listening on %u\n", static_cast<unsigned>(port));
    return std::make_unique<TcpListener>(server, port);
}
