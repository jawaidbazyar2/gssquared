#pragma once

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm> // Required for remove_if
#include <cctype>
#include <atomic>

#include "serial_devices/SerialDevice.hpp"
#include "serial_devices/host/HostSerial.hpp"
#include "util/printf_helper.hpp"

class ModemDevice : public SerialDevice {
    private:
        enum ModemState {
            STATE_COMMAND,      // Waiting for AT commands
            STATE_ONLINE,       // Connected to remote host
            STATE_ESCAPE        // Processing +++ escape sequence
        };

        // Telnet protocol constants
        enum TelnetCommand {
            IAC  = 255,  // Interpret As Command
            DONT = 254,  // Don't do option
            DO   = 253,  // Do option
            WONT = 252,  // Won't do option
            WILL = 251,  // Will do option
            SB   = 250,  // Subnegotiation begin
            SE   = 240   // Subnegotiation end
        };

        // Telnet options
        enum TelnetOption {
            TELOPT_BINARY = 0,         // Binary transmission
            TELOPT_ECHO = 1,           // Echo option
            TELOPT_SUPPRESS_GO_AHEAD = 3  // Suppress Go Ahead
        };

        enum TelnetState {
            TELNET_DATA,     // Normal data
            TELNET_IAC,      // Received IAC
            TELNET_NEGOTIATE // Received IAC + command, waiting for option
        };

        ModemState state;
        std::atomic<int> hud_state_{STATE_COMMAND};
        char hud_last_cmd_[40]{};
        char hud_peer_[48]{};
        std::atomic<bool> hud_ringing_{false};
        std::string command_buffer;
        NET_StreamSocket *socket;          /* answered / dialed session — CD follows this */
        NET_StreamSocket *pending_socket;  /* inbound, not yet ATA */
        NET_Server *server;
        std::string remote_host;
        int remote_port;
        bool command_echo = true;
        bool verbose_ = true; /* ATV1 words; ATV0 numeric */
        uint8_t result_x_ = 0; /* ATXn: 0 = CONNECT only; 1+ = CONNECT <baud> */
        uint8_t s0_ = 0;       /* ATS0=n auto-answer after n RINGs; 0 = off */
        uint8_t ring_count_ = 0;
        uint32_t line_baud_ = 9600;     /* last DTE MESSAGE_LINE */
        uint32_t connect_baud_ = 9600;  /* DTE baud at CONNECT; ATO must not follow later UART writes */
        
        // Escape sequence detection
        uint64_t last_char_time;
        uint64_t last_ring_time;
        int escape_count;
        constexpr static uint64_t ESCAPE_GUARD_TIME_MS = 1000; // 1 second guard time
        constexpr static uint16_t LISTEN_PORT = 6502;
        constexpr static uint64_t RING_INTERVAL_MS = 3000;
        
        // Telnet protocol handling
        TelnetState telnet_state;
        uint8_t telnet_command;
        /* Inbound call: we are the telnet server, so we echo and drive the
         * client into character-at-a-time mode. Outbound dial leaves the
         * stream alone. NVT line-end bridging applies per RFC 856: only on a
         * direction that is *not* in binary mode, so ZMODEM and other 8-bit
         * transfers stay byte-transparent. */
        bool server_role_ = false;
        bool binary_tx_ = false;  /* peer agreed DO BINARY: we may send raw */
        bool binary_rx_ = false;  /* peer agreed WILL BINARY: it sends raw */
        bool remote_saw_cr_ = false;
        bool guest_saw_cr_ = false;
        
        // TCP receive buffer
        std::vector<uint8_t> tcp_buffer;

        void send_response(const char *response) {
            size_t len = strlen(response);
            for (size_t i = 0; i < len; i++) {
                SerialMessage msg;
                msg.type = MESSAGE_DATA;
                msg.data = response[i];
                int tries = 0;
                while (!q_dev.send(msg) && tries++ < 50) {
                    SDL_Delay(2);
                }
            }
        }

        uint8_t connect_result_code(uint32_t baud) const {
            /* Hayes Smartmodem V0 + GBBS: 1/5/10/11/12/14, later 28. */
            switch (baud) {
                case 1200: return 5;
                case 2400: return 10;
                case 4800: return 11;
                case 9600: return 12;
                case 19200: return 14;
                case 38400: return 28;
                default: return 1; /* CONNECT 300 / unknown */
            }
        }

        void send_result(uint8_t code) {
            if (!verbose_) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "\r\n%u\r\n", static_cast<unsigned>(code));
                send_response(buf);
                return;
            }
            switch (code) {
                case 0:
                    send_response("\r\nOK\r\n");
                    break;
                case 2:
                    send_response("\r\nRING\r\n");
                    break;
                case 3:
                    send_response("\r\nNO CARRIER\r\n");
                    break;
                case 4:
                    send_response("\r\nERROR\r\n");
                    break;
                default: {
                    const uint32_t baud = connect_baud_ ? connect_baud_
                                                        : (line_baud_ ? line_baud_ : 9600u);
                    char connect[32];
                    std::snprintf(connect, sizeof(connect), "\r\nCONNECT %u\r\n", baud);
                    send_response(connect);
                    break;
                }
            }
        }

        void send_ok_response() {
            send_result(0);
        }

        void send_error_response() {
            send_result(4);
        }

        void send_no_carrier_response() {
            send_result(3);
        }

        void push_modem_inputs() {
            uint8_t bits = MODEM_CTS | MODEM_DSR;
            if (socket) {
                bits |= MODEM_CD;
            }
            set_modem_inputs(bits);
        }

        /* Fresh telnet session: nothing negotiated, no half-seen CR. */
        void reset_telnet(bool server_role) {
            telnet_state = TELNET_DATA;
            server_role_ = server_role;
            binary_tx_ = false;
            binary_rx_ = false;
            remote_saw_cr_ = false;
            guest_saw_cr_ = false;
        }

        void set_state(ModemState s) {
            state = s;
            hud_state_.store(static_cast<int>(s), std::memory_order_relaxed);
        }

        void note_hud_cmd(const std::string &cmd) {
            std::snprintf(hud_last_cmd_, sizeof(hud_last_cmd_), "%s", cmd.c_str());
        }

        void note_hud_peer() {
            if (socket || pending_socket) {
                std::snprintf(hud_peer_, sizeof(hud_peer_), "%s:%d", remote_host.c_str(),
                              remote_port);
            } else {
                hud_peer_[0] = '\0';
            }
        }

        void capture_peer_from_stream(NET_StreamSocket *s) {
            remote_host = "inbound";
            remote_port = LISTEN_PORT;
            if (!s) {
                return;
            }
            NET_Address *addr = NET_GetStreamSocketAddress(s);
            if (addr) {
                const char *str = NET_GetAddressString(addr);
                if (str && str[0] != '\0') {
                    remote_host = str;
                }
                NET_UnrefAddress(addr);
            }
        }

        NET_StreamSocket *active_stream() {
            return socket ? socket : pending_socket;
        }

        void drop_pending() {
            if (pending_socket) {
                printf("ModemDevice: dropping pending inbound\n");
                NET_DestroyStreamSocket(pending_socket);
                pending_socket = nullptr;
            }
            hud_ringing_.store(false, std::memory_order_relaxed);
            ring_count_ = 0;
            if (!socket) {
                reset_telnet(false);
                tcp_buffer.clear();
                note_hud_peer();
            }
        }

        void start_listen() {
            server = NET_CreateServer(nullptr, LISTEN_PORT);
            if (!server) {
                printf("ModemDevice: listen %u failed: %s (outbound only)\n",
                       static_cast<unsigned>(LISTEN_PORT), SDL_GetError());
            } else {
                printf("ModemDevice: listening on %u\n", static_cast<unsigned>(LISTEN_PORT));
            }
        }

        void stop_listen() {
            if (server) {
                NET_DestroyServer(server);
                server = nullptr;
            }
        }

        void send_ring() {
            send_result(2);
            last_ring_time = SDL_GetTicks();
            if (ring_count_ < 255) {
                ring_count_++;
            }
            printf("ModemDevice: RING %u (S0=%u)\n", static_cast<unsigned>(ring_count_),
                   static_cast<unsigned>(s0_));
            if (s0_ > 0 && ring_count_ >= s0_) {
                answer();
            }
        }

        void poll_incoming() {
            if (!server) {
                return;
            }
            NET_StreamSocket *incoming = nullptr;
            if (!NET_AcceptClient(server, &incoming)) {
                printf("ModemDevice: AcceptClient failed: %s\n", SDL_GetError());
                return;
            }
            if (!incoming) {
                return;
            }
            if (socket || pending_socket || state != STATE_COMMAND) {
                printf("ModemDevice: rejecting inbound (busy)\n");
                NET_DestroyStreamSocket(incoming);
                return;
            }
            pending_socket = incoming;
            hud_ringing_.store(true, std::memory_order_relaxed);
            reset_telnet(true);
            tcp_buffer.clear();
            capture_peer_from_stream(pending_socket);
            note_hud_peer();
            ring_count_ = 0;
            send_ring();
        }

        void poll_pending() {
            if (!pending_socket) {
                return;
            }

            const uint64_t now = SDL_GetTicks();
            if (now - last_ring_time >= RING_INTERVAL_MS) {
                send_ring();
                if (!pending_socket) {
                    return;
                }
            }

            uint8_t buffer[256];
            const int to_read = (tcp_buffer.size() >= 4096) ? 1 : static_cast<int>(sizeof(buffer));
            int bytes_read = NET_ReadFromStreamSocket(pending_socket, buffer, to_read);
            if (bytes_read > 0) {
                for (int i = 0; i < bytes_read; i++) {
                    process_telnet_byte(buffer[i]);
                }
                /* Hold payload until ATA — do not drain to the guest. */
            } else if (bytes_read < 0) {
                printf("ModemDevice: inbound caller hung up before answer\n");
                drop_pending();
            }
        }

        void process_command() {
            std::string cmd = command_buffer;
            for (auto &c : cmd) {
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            }

            printf("ModemDevice: Processing command: %s\n", cmd.c_str());
            note_hud_cmd(cmd);

            if (cmd.length() >= 2 && cmd.substr(0, 2) == "AT") {
                cmd = cmd.substr(2);
            }

            if (cmd.empty()) {
                send_ok_response();
                command_buffer.clear();
                return;
            }

            /* Hayes: ATE0V1H is echo-off, verbose-on, hang-up — walk the whole line. */
            bool ok = true;
            size_t i = 0;
            while (ok && i < cmd.size()) {
                while (i < cmd.size() && isspace(static_cast<unsigned char>(cmd[i]))) {
                    i++;
                }
                if (i >= cmd.size()) {
                    break;
                }
                const char c = cmd[i++];
                switch (c) {
                    case 'A': {
                        /* ATA / ATA0 — answer inbound RING. */
                        if (i < cmd.size() && cmd[i] == '0') {
                            i++;
                        }
                        command_buffer.clear();
                        answer();
                        return;
                    }
                    case 'D': {
                        std::string address = cmd.substr(i);
                        if (!address.empty() && (address[0] == 'T' || address[0] == 'P')) {
                            address = address.substr(1);
                        }
                        address.erase(remove_if(address.begin(), address.end(), ::isspace),
                                      address.end());
                        command_buffer.clear();
                        if (address.empty()) {
                            send_error_response();
                        } else {
                            dial(address);
                        }
                        return;
                    }
                    case 'H':
                        if (i < cmd.size() && (cmd[i] == '0' || cmd[i] == '1')) {
                            i++;
                        }
                        hangup();
                        break;
                    case 'O': {
                        /* ATO / ATO0: back to data. ATO1 is retrain — same here. */
                        if (i < cmd.size() && (cmd[i] == '0' || cmd[i] == '1')) {
                            i++;
                        }
                        command_buffer.clear();
                        go_online();
                        return;
                    }
                    case 'Z':
                        hangup();
                        break;
                    case 'E':
                        if (i < cmd.size() && cmd[i] == '0') {
                            command_echo = false;
                            i++;
                        } else if (i < cmd.size() && cmd[i] == '1') {
                            command_echo = true;
                            i++;
                        } else {
                            ok = false;
                        }
                        break;
                    case 'V':
                        /* ATV / ATV0 = numeric; ATV1 = words. */
                        if (i < cmd.size() && cmd[i] == '0') {
                            verbose_ = false;
                            i++;
                        } else if (i < cmd.size() && cmd[i] == '1') {
                            verbose_ = true;
                            i++;
                        } else if (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            ok = false;
                        } else {
                            verbose_ = false;
                        }
                        break;
                    case 'Q':
                    case 'N': /* automode */
                    case 'M': /* speaker */
                    case 'L': /* volume */
                    case 'W': /* negotiation progress */
                        while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            i++;
                        }
                        break;
                    case 'X': /* extended result codes — ProTERM matches CONNECT / CONNECT 9600 */
                        if (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            result_x_ = static_cast<uint8_t>(cmd[i] - '0');
                            i++;
                            while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                                i++;
                            }
                        }
                        break;
                    case '&': {
                        /* AT&F factory reset; other &x[n] accepted as no-ops (ProTERM init). */
                        if (i >= cmd.size() || !isalpha(static_cast<unsigned char>(cmd[i]))) {
                            ok = false;
                            break;
                        }
                        const char amp = cmd[i++];
                        while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            i++;
                        }
                        if (amp == 'F') {
                            hangup();
                            command_echo = true;
                            verbose_ = true;
                            result_x_ = 0;
                            s0_ = 0;
                        }
                        break;
                    }
                    case 'S': {
                        /* Sn / Sn=value / Sn? — S0 is auto-answer rings; others accepted no-ops. */
                        if (i >= cmd.size() || !isdigit(static_cast<unsigned char>(cmd[i]))) {
                            ok = false;
                            break;
                        }
                        unsigned reg = 0;
                        while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            const unsigned d = static_cast<unsigned>(cmd[i++] - '0');
                            reg = (reg > 25) ? 255u : (reg * 10u + d);
                        }
                        if (i < cmd.size() && cmd[i] == '?') {
                            i++;
                            char buf[16];
                            if (reg == 0) {
                                std::snprintf(buf, sizeof(buf), "%u\r\n",
                                              static_cast<unsigned>(s0_));
                            } else {
                                std::snprintf(buf, sizeof(buf), "255\r\n");
                            }
                            send_response(buf);
                        } else if (i < cmd.size() && cmd[i] == '=') {
                            i++;
                            bool neg = false;
                            if (i < cmd.size() && cmd[i] == '-') {
                                neg = true;
                                i++;
                            }
                            if (i >= cmd.size() || !isdigit(static_cast<unsigned char>(cmd[i]))) {
                                ok = false;
                                break;
                            }
                            unsigned val = 0;
                            while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                                const unsigned d = static_cast<unsigned>(cmd[i++] - '0');
                                val = (val > 25) ? 255u : (val * 10u + d);
                            }
                            if (neg) {
                                val = 0;
                            }
                            if (reg == 0) {
                                s0_ = static_cast<uint8_t>(val);
                            }
                        }
                        break;
                    }
                    default:
                        ok = false;
                        break;
                }
            }

            if (ok) {
                send_ok_response();
            } else {
                send_error_response();
            }
            command_buffer.clear();
        }

        void dial(const std::string &address) {
            printf("ModemDevice: Dialing %s\n", address.c_str());
            
            // Close existing session or unanswered inbound
            if (socket || pending_socket) {
                hangup();
            }

            // Parse address - expecting IP address, port defaults to 23 (telnet)
            remote_host = address;
            remote_port = 23;

            // Check if there's a port specified (after a colon)
            size_t colon_pos = address.find(':');
            if (colon_pos != std::string::npos) {
                remote_host = address.substr(0, colon_pos);
                try {
                    remote_port = std::stoi(address.substr(colon_pos + 1));
                } catch (...) {
                    send_error_response();
                    return;
                }
            }

            printf("ModemDevice: Connecting to %s:%d\n", remote_host.c_str(), remote_port);

            // Resolve hostname (asynchronous)
            NET_Address *addr = NET_ResolveHostname(remote_host.c_str());
            if (!addr) {
                printf("ModemDevice: Failed to start hostname resolution: %s\n", SDL_GetError());
                send_no_carrier_response();
                return;
            }

            // Wait for resolution to complete (10 second timeout)
            NET_Status resolve_status = NET_WaitUntilResolved(addr, 10000);
            if (resolve_status != NET_SUCCESS) {
                printf("ModemDevice: Failed to resolve hostname: %s\n", SDL_GetError());
                NET_UnrefAddress(addr);
                send_no_carrier_response();
                return;
            }

            // Create client socket (starts connection asynchronously)
            socket = NET_CreateClient(addr, remote_port);
            NET_UnrefAddress(addr);

            if (!socket) {
                printf("ModemDevice: Failed to create client: %s\n", SDL_GetError());
                send_no_carrier_response();
                return;
            }

            // Wait for connection to complete (30 second timeout)
            NET_Status connect_status = NET_WaitUntilConnected(socket, 30000);
            if (connect_status != NET_SUCCESS) {
                printf("ModemDevice: Failed to connect: %s\n", SDL_GetError());
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
                send_no_carrier_response();
                return;
            }

            // Connection successful. Raise CD before CONNECT — ProTERM (and SCC
            // auto-enables) treat carrier as the online gate; result text after
            // DCD is still low is often ignored.
            set_state(STATE_ONLINE);
            reset_telnet(false);  // We are the telnet client here
            tcp_buffer.clear();   // Clear any buffered data
            note_hud_peer();
            push_modem_inputs();
            SDL_Delay(20); /* let the UART sample CD before CONNECT is queued */

            // Negotiate binary mode in both directions for 8-bit transparency
            send_telnet_response(WILL, TELOPT_BINARY);  // We will send binary
            send_telnet_response(DO, TELOPT_BINARY);    // Please send us binary

            connect_baud_ = line_baud_ ? line_baud_ : 9600u;
            send_connect_response();
            printf("ModemDevice: Connected!\n");
        }

        void send_connect_response() {
            /* Only CONNECT <baud>. A bare CONNECT is Hayes 300; ProTERM's
             * Smartmodem driver then drops the title-bar rate to 300. */
            const uint32_t baud = connect_baud_ ? connect_baud_
                                                : (line_baud_ ? line_baud_ : 9600u);
            const uint8_t code = connect_result_code(baud);
            printf("ModemDevice: sending to guest: CONNECT %u (result %u)\n", baud,
                   static_cast<unsigned>(code));
            send_result(code);
        }

        void go_online() {
            if (!socket) {
                send_no_carrier_response();
                set_state(STATE_COMMAND);
                return;
            }
            printf("ModemDevice: ATO — returning to data mode\n");
            set_state(STATE_ONLINE);
            escape_count = 0;
            push_modem_inputs();
            send_connect_response();
        }

        void answer() {
            if (!pending_socket) {
                send_no_carrier_response();
                set_state(STATE_COMMAND);
                return;
            }
            printf("ModemDevice: ATA — answering inbound\n");
            socket = pending_socket;
            pending_socket = nullptr;
            hud_ringing_.store(false, std::memory_order_relaxed);
            ring_count_ = 0;
            set_state(STATE_ONLINE);
            escape_count = 0;
            note_hud_peer();
            push_modem_inputs();
            SDL_Delay(20); /* let the UART sample CD before CONNECT is queued */

            /* Server-side offers. Without WILL ECHO + WILL SGA a BSD telnet
             * client stays in line mode with local echo: GBBS sees nothing
             * until Return, and single-keystroke menus never fire. */
            send_telnet_response(WILL, TELOPT_BINARY);
            send_telnet_response(DO, TELOPT_BINARY);
            send_telnet_response(WILL, TELOPT_ECHO);
            send_telnet_response(WILL, TELOPT_SUPPRESS_GO_AHEAD);
            send_telnet_response(DO, TELOPT_SUPPRESS_GO_AHEAD);

            connect_baud_ = line_baud_ ? line_baud_ : 9600u;
            send_connect_response();
            drain_tcp_buffer_to_queue();
            printf("ModemDevice: Connected (inbound)!\n");
        }

        void hangup() {
            if (socket) {
                printf("ModemDevice: Hanging up\n");
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
            }
            drop_pending();
            push_modem_inputs();
            set_state(STATE_COMMAND);
            reset_telnet(false);  // Reset telnet protocol state
            tcp_buffer.clear();   // Clear any buffered data
            note_hud_peer();
        }

        void handle_escape_sequence() {
            printf("ModemDevice: Escape sequence detected, returning to command mode\n");
            set_state(STATE_COMMAND);
            send_ok_response();
            escape_count = 0;
        }

        void send_telnet_response(uint8_t command, uint8_t option) {
            NET_StreamSocket *s = active_stream();
            if (!s) {
                return;
            }
            uint8_t response[3] = { IAC, command, option };
            NET_WriteToStreamSocket(s, response, 3);
        }

        void push_remote_byte(uint8_t byte) {
            if (binary_rx_ || !server_role_) {
                tcp_buffer.push_back(byte);  // transparent: no NVT rewriting
                return;
            }
            if (remote_saw_cr_) {
                remote_saw_cr_ = false;
                /* NVT sends CR LF or CR NUL for Return; the Apple II wants
                 * the bare CR, and a stray NUL confuses GBBS input. */
                if (byte == 0x00 || byte == 0x0A) {
                    return;
                }
            }
            remote_saw_cr_ = (byte == 0x0D);
            tcp_buffer.push_back(byte);
        }

        void process_telnet_byte(uint8_t byte) {
            switch (telnet_state) {
                case TELNET_DATA:
                    if (byte == IAC) {
                        telnet_state = TELNET_IAC;
                    } else {
                        // Normal data - buffer it
                        push_remote_byte(byte);
                    }
                    break;

                case TELNET_IAC:
                    if (byte == IAC) {
                        // Escaped IAC - buffer single 0xFF
                        push_remote_byte(IAC);
                        telnet_state = TELNET_DATA;
                    } else if (byte == WILL || byte == WONT || byte == DO || byte == DONT) {
                        // Negotiation command - wait for option byte
                        telnet_command = byte;
                        telnet_state = TELNET_NEGOTIATE;
                    } else if (byte == SB) {
                        // Subnegotiation - for now, just ignore (would need more state)
                        telnet_state = TELNET_DATA;
                    } else {
                        // Other commands - ignore and return to data mode
                        telnet_state = TELNET_DATA;
                    }
                    break;

                case TELNET_NEGOTIATE:
                    // Received option byte - respond appropriately
                    if (telnet_command == WILL) {
                        // Peer wants to enable option on its side. Only a
                        // dialed-out server may echo for us; an inbound caller
                        // must not, since the BBS is doing the echoing.
                        const bool accept = (byte == TELOPT_BINARY ||
                                             byte == TELOPT_SUPPRESS_GO_AHEAD ||
                                             (!server_role_ && byte == TELOPT_ECHO));
                        send_telnet_response(accept ? DO : DONT, byte);
                        if (byte == TELOPT_BINARY) {
                            binary_rx_ = accept;
                        }
                    } else if (telnet_command == DO) {
                        // Peer wants us to enable option. ECHO only as server;
                        // a telnet client must never echo the host back.
                        const bool accept = (byte == TELOPT_BINARY ||
                                             byte == TELOPT_SUPPRESS_GO_AHEAD ||
                                             (server_role_ && byte == TELOPT_ECHO));
                        send_telnet_response(accept ? WILL : WONT, byte);
                        if (byte == TELOPT_BINARY) {
                            binary_tx_ = accept;
                        }
                    } else if (telnet_command == WONT && byte == TELOPT_BINARY) {
                        binary_rx_ = false;  // peer will not send us binary
                    } else if (telnet_command == DONT && byte == TELOPT_BINARY) {
                        binary_tx_ = false;  // peer will not take binary from us
                    }
                    telnet_state = TELNET_DATA;
                    break;
            }
        }

        void drain_tcp_buffer_to_queue() {
            // Try to move buffered data to the serial queue
            while (!tcp_buffer.empty() && !q_dev.is_full()) {
                SerialMessage msg;
                msg.type = MESSAGE_DATA;
                msg.data = tcp_buffer[0];
                if (q_dev.send(msg)) {
                    tcp_buffer.erase(tcp_buffer.begin());
                } else {
                    break;  // Queue is full, stop trying
                }
            }
        }

        void check_tcp_data() {
            if (!socket) return;

            // First, try to drain any buffered data to the queue
            drain_tcp_buffer_to_queue();

            // Only read more from TCP if our buffer isn't too large (4KB max)
            if (tcp_buffer.size() >= 4096) {
                return;  // Buffer full, wait for queue to drain
            }

            // Try to receive data from TCP connection
            uint8_t buffer[256];
            int bytes_read = NET_ReadFromStreamSocket(socket, buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                // Process received data through telnet protocol handler
                for (int i = 0; i < bytes_read; i++) {
                    process_telnet_byte(buffer[i]);
                }
                
                // Try to drain buffer again after adding new data
                drain_tcp_buffer_to_queue();
            } else if (bytes_read < 0) {
                // Error or connection closed
                printf("ModemDevice: Connection lost\n");
                send_no_carrier_response();
                hangup();
            }
        }

        void write_stream(const uint8_t *data, int len) {
            if (!NET_WriteToStreamSocket(socket, data, len)) {
                printf("ModemDevice: Failed to send data: %s\n", SDL_GetError());
                send_no_carrier_response();
                hangup();
            }
        }

        void send_tcp_data(uint8_t byte) {
            if (!socket) return;

            if (server_role_ && !binary_tx_) {
                /* NVT: a bare CR is illegal, and a client that refused binary
                 * needs the LF or the next line overwrites this one. */
                const bool after_cr = guest_saw_cr_;
                guest_saw_cr_ = (byte == 0x0D);
                if (after_cr && byte == 0x0A) {
                    return; /* guest already sent CR LF */
                }
                if (byte == 0x0D) {
                    const uint8_t crlf[2] = { 0x0D, 0x0A };
                    write_stream(crlf, 2);
                    return;
                }
            }

            // If byte is IAC (0xFF), we need to escape it by sending IAC IAC
            if (byte == IAC) {
                const uint8_t escaped[2] = { IAC, IAC };
                write_stream(escaped, 2);
            } else {
                write_stream(&byte, 1);
            }
        }

    public:
        void format_hud_status(char *buf, size_t n) const override {
            char hs[24];
            format_handshake(hs, sizeof(hs));
            const char *mode = "CMD";
            if (hud_ringing_.load(std::memory_order_relaxed)) {
                mode = "RNG";
            } else {
                switch (hud_state_.load(std::memory_order_relaxed)) {
                    case STATE_ONLINE: mode = "ONL"; break;
                    case STATE_ESCAPE: mode = "ESC"; break;
                    default: break;
                }
            }
            if (hud_peer_[0] != '\0' && hud_last_cmd_[0] != '\0') {
                std::snprintf(buf, n, "%s %s %s [%s]", mode, hs, hud_peer_, hud_last_cmd_);
            } else if (hud_peer_[0] != '\0') {
                std::snprintf(buf, n, "%s %s %s", mode, hs, hud_peer_);
            } else if (hud_last_cmd_[0] != '\0') {
                std::snprintf(buf, n, "%s %s [%s]", mode, hs, hud_last_cmd_);
            } else {
                std::snprintf(buf, n, "%s %s", mode, hs);
            }
        }

        ModemDevice(const char *name, const char *port_id) : SerialDevice("ModemDevice", port_id), 
                       state(STATE_COMMAND),
                       socket(nullptr),
                       pending_socket(nullptr),
                       server(nullptr),
                       remote_port(23),
                       last_char_time(0),
                       last_ring_time(0),
                       escape_count(0),
                       telnet_state(TELNET_DATA),
                       telnet_command(0) {
            set_modem_inputs(MODEM_CTS | MODEM_DSR);
            if (!NET_Init()) {
                printf("ModemDevice: Failed to initialize SDL_net: %s\n", SDL_GetError());
            } else {
                printf("ModemDevice: Initialized\n");
            }
        }

        ~ModemDevice() {
            // Ensure thread stops before our members are destroyed
            if (thread) {
                SDL_Log("SerialDevice: %s shutting down", this->name);
                SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
                q_host.send(msg);
                SDL_WaitThread(thread, NULL);
                thread = nullptr;
            }
            
            if (socket) {
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
            }
            if (pending_socket) {
                NET_DestroyStreamSocket(pending_socket);
                pending_socket = nullptr;
            }
            if (server) {
                NET_DestroyServer(server);
                server = nullptr;
            }
            NET_Quit();
        }

        void device_loop() override {
            start_listen();
            push_modem_inputs();
            while (true) {
                SDL_Delay(10); // Check every 10ms for better responsiveness

                poll_incoming();
                if (state == STATE_ONLINE) {
                    check_tcp_data();
                } else if (pending_socket) {
                    poll_pending();
                }
                
                // Process host messages
                while (!q_host.is_empty()) {
                    SerialMessage msg = q_host.get();
                    
                    switch (msg.type) {
                        case MESSAGE_SHUTDOWN:
                            printf("ModemDevice: shutting down\n");
                            hangup();
                            stop_listen();
                            return;

                        case MESSAGE_LINE: {
                            uint32_t b = host_serial_unpack_line(msg.data).baud;
                            if (b == 0) {
                                b = 9600;
                            }
                            if (b != line_baud_) {
                                printf("ModemDevice: DTE baud %u -> %u (connect stays %u)\n",
                                       line_baud_, b, connect_baud_);
                            }
                            line_baud_ = b;
                            break;
                        }
                            
                        case MESSAGE_DATA: {
                            uint8_t byte = static_cast<uint8_t>(msg.data);
                            uint64_t current_time = SDL_GetTicks();
                            
                            if (state == STATE_COMMAND) {
                                const uint8_t ch = byte & 0x7F;
                                if (command_echo) q_dev.send({MESSAGE_DATA, byte});

                                // Command mode - process AT commands
                                if (ch == '\r' || ch == '\n') {
                                    if (!command_buffer.empty()) {
                                        process_command();
                                    }
                                } else if (ch == 8 || ch == 127) {
                                    // Backspace or DEL
                                    if (!command_buffer.empty()) {
                                        command_buffer.pop_back();
                                    }
                                } else if (ch >= 32 && ch < 127) {
                                    // Printable character
                                    command_buffer += static_cast<char>(ch);
                                }
                            } else if (state == STATE_ONLINE) {
                                // Online mode - check for escape sequence
                                if (byte == '+') {
                                    // Check guard time before first '+'
                                    if (escape_count == 0) {
                                        if (current_time - last_char_time >= ESCAPE_GUARD_TIME_MS) {
                                            escape_count = 1;
                                        } else {
                                            // Too soon, send it through
                                            send_tcp_data(byte);
                                        }
                                    } else if (escape_count < 3) {
                                        // Check that '+' characters come quickly
                                        if (current_time - last_char_time < 500) {
                                            escape_count++;
                                            if (escape_count == 3) {
                                                // Got +++, but need guard time after
                                                set_state(STATE_ESCAPE);
                                            }
                                        } else {
                                            // Too slow, reset and send all buffered '+'
                                            for (int i = 0; i < escape_count; i++) {
                                                send_tcp_data('+');
                                            }
                                            send_tcp_data(byte);
                                            escape_count = 0;
                                        }
                                    }
                                } else {
                                    // Not a '+', check if we were in escape sequence
                                    if (escape_count > 0) {
                                        // Send buffered '+' characters
                                        for (int i = 0; i < escape_count; i++) {
                                            send_tcp_data('+');
                                        }
                                        escape_count = 0;
                                    }
                                    // Send the data
                                    send_tcp_data(byte);
                                }
                            } else if (state == STATE_ESCAPE) {
                                // We received +++, waiting for guard time
                                // If any character comes in too soon, abort escape
                                if (current_time - last_char_time < ESCAPE_GUARD_TIME_MS) {
                                    // Guard time violated, send +++ and this char
                                    for (int i = 0; i < 3; i++) {
                                        send_tcp_data('+');
                                    }
                                    send_tcp_data(byte);
                                    set_state(STATE_ONLINE);
                                    escape_count = 0;
                                }
                            }
                            
                            last_char_time = current_time;
                            break;
                        }
                        
                        default:
                            break;
                    }
                }
                
                // Check if escape sequence completed (guard time after +++)
                if (state == STATE_ESCAPE) {
                    uint64_t current_time = SDL_GetTicks();
                    if (current_time - last_char_time >= ESCAPE_GUARD_TIME_MS) {
                        handle_escape_sequence();
                    }
                }
            }
        }
};
