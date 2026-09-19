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
        std::string command_buffer;
        NET_StreamSocket *socket;
        std::string remote_host;
        int remote_port;
        bool command_echo = true;
        uint8_t result_x_ = 0; /* ATXn: 0 = CONNECT only; 1+ = CONNECT <baud> */
        uint32_t line_baud_ = 9600;
        
        // Escape sequence detection
        uint64_t last_char_time;
        int escape_count;
        constexpr static uint64_t ESCAPE_GUARD_TIME_MS = 1000; // 1 second guard time
        
        // Telnet protocol handling
        TelnetState telnet_state;
        uint8_t telnet_command;
        
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

        void send_ok_response() {
            send_response("\r\nOK\r\n");
        }

        void send_error_response() {
            send_response("\r\nERROR\r\n");
        }

        void send_no_carrier_response() {
            send_response("\r\nNO CARRIER\r\n");
        }

        void push_modem_inputs() {
            uint8_t bits = MODEM_CTS | MODEM_DSR;
            if (socket) {
                bits |= MODEM_CD;
            }
            set_modem_inputs(bits);
        }

        void set_state(ModemState s) {
            state = s;
            hud_state_.store(static_cast<int>(s), std::memory_order_relaxed);
        }

        void note_hud_cmd(const std::string &cmd) {
            std::snprintf(hud_last_cmd_, sizeof(hud_last_cmd_), "%s", cmd.c_str());
        }

        void note_hud_peer() {
            if (socket) {
                std::snprintf(hud_peer_, sizeof(hud_peer_), "%s:%d", remote_host.c_str(),
                              remote_port);
            } else {
                hud_peer_[0] = '\0';
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
                            result_x_ = 0;
                        }
                        break;
                    }
                    case 'S': {
                        /* Sn or Sn=value / Sn? — store nothing; ProTERM sets S7 wait-for-carrier. */
                        if (i >= cmd.size() || !isdigit(static_cast<unsigned char>(cmd[i]))) {
                            ok = false;
                            break;
                        }
                        while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                            i++;
                        }
                        if (i < cmd.size() && cmd[i] == '?') {
                            i++;
                            send_response("255\r\n");
                        } else if (i < cmd.size() && cmd[i] == '=') {
                            i++;
                            if (i < cmd.size() && cmd[i] == '-') {
                                i++;
                            }
                            if (i >= cmd.size() || !isdigit(static_cast<unsigned char>(cmd[i]))) {
                                ok = false;
                                break;
                            }
                            while (i < cmd.size() && isdigit(static_cast<unsigned char>(cmd[i]))) {
                                i++;
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
            
            // Close existing connection if any
            if (socket) {
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
            telnet_state = TELNET_DATA;  // Reset telnet protocol state
            tcp_buffer.clear();  // Clear any buffered data
            note_hud_peer();
            push_modem_inputs();
            SDL_Delay(20); /* let the UART sample CD before CONNECT is queued */

            // Negotiate binary mode in both directions for 8-bit transparency
            send_telnet_response(WILL, TELOPT_BINARY);  // We will send binary
            send_telnet_response(DO, TELOPT_BINARY);    // Please send us binary

            /* ProTERM matches CONNECT text, not DCD. Bare CONNECT covers X0 and
             * substring parsers; CONNECT <baud> covers speed-specific drivers. */
            char connect[48];
            std::snprintf(connect, sizeof(connect), "\r\nCONNECT\r\nCONNECT %u\r\n",
                          line_baud_ ? line_baud_ : 9600u);
            printf("ModemDevice: sending to guest: CONNECT / CONNECT %u\n",
                   line_baud_ ? line_baud_ : 9600u);
            send_response(connect);
            printf("ModemDevice: Connected!\n");
        }

        void hangup() {
            if (socket) {
                printf("ModemDevice: Hanging up\n");
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
            }
            push_modem_inputs();
            set_state(STATE_COMMAND);
            telnet_state = TELNET_DATA;  // Reset telnet protocol state
            tcp_buffer.clear();  // Clear any buffered data
            note_hud_peer();
        }

        void handle_escape_sequence() {
            printf("ModemDevice: Escape sequence detected, returning to command mode\n");
            set_state(STATE_COMMAND);
            send_ok_response();
            escape_count = 0;
        }

        void send_telnet_response(uint8_t command, uint8_t option) {
            uint8_t response[3] = { IAC, command, option };
            NET_WriteToStreamSocket(socket, response, 3);
        }

        void process_telnet_byte(uint8_t byte) {
            switch (telnet_state) {
                case TELNET_DATA:
                    if (byte == IAC) {
                        telnet_state = TELNET_IAC;
                    } else {
                        // Normal data - buffer it
                        tcp_buffer.push_back(byte);
                    }
                    break;

                case TELNET_IAC:
                    if (byte == IAC) {
                        // Escaped IAC - buffer single 0xFF
                        tcp_buffer.push_back(IAC);
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
                        // Server wants to enable option
                        if (byte == TELOPT_BINARY || byte == TELOPT_ECHO || byte == TELOPT_SUPPRESS_GO_AHEAD) {
                            // Accept BINARY, ECHO, and SUPPRESS_GO_AHEAD
                            send_telnet_response(DO, byte);
                        } else {
                            // Refuse other options
                            send_telnet_response(DONT, byte);
                        }
                    } else if (telnet_command == DO) {
                        // Server wants us to enable option
                        if (byte == TELOPT_BINARY) {
                            // Accept BINARY mode
                            send_telnet_response(WILL, byte);
                        } else {
                            // Refuse other options
                            send_telnet_response(WONT, byte);
                        }
                    }
                    // For WONT and DONT, no response needed
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
                send_response("\r\nNO CARRIER\r\n");
                hangup();
            }
        }

        void send_tcp_data(uint8_t byte) {
            if (!socket) return;

            // If byte is IAC (0xFF), we need to escape it by sending IAC IAC
            if (byte == IAC) {
                uint8_t escaped[2] = { IAC, IAC };
                bool result = NET_WriteToStreamSocket(socket, escaped, 2);
                if (!result) {
                    printf("ModemDevice: Failed to send data: %s\n", SDL_GetError());
                    send_response("\r\nNO CARRIER\r\n");
                    hangup();
                }
            } else {
                bool result = NET_WriteToStreamSocket(socket, &byte, 1);
                if (!result) {
                    printf("ModemDevice: Failed to send data: %s\n", SDL_GetError());
                    send_response("\r\nNO CARRIER\r\n");
                    hangup();
                }
            }
        }

    public:
        void format_hud_status(char *buf, size_t n) const override {
            char hs[24];
            format_handshake(hs, sizeof(hs));
            const char *mode = "CMD";
            switch (hud_state_.load(std::memory_order_relaxed)) {
                case STATE_ONLINE: mode = "ONL"; break;
                case STATE_ESCAPE: mode = "ESC"; break;
                default: break;
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
                       remote_port(23),
                       last_char_time(0),
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
            NET_Quit();
        }

        void device_loop() override {
            push_modem_inputs();
            while (true) {
                SDL_Delay(10); // Check every 10ms for better responsiveness
                
                // Check for incoming TCP data if we're online
                if (state == STATE_ONLINE) {
                    check_tcp_data();
                }
                
                // Process host messages
                while (!q_host.is_empty()) {
                    SerialMessage msg = q_host.get();
                    
                    switch (msg.type) {
                        case MESSAGE_SHUTDOWN:
                            printf("ModemDevice: shutting down\n");
                            hangup();
                            return;

                        case MESSAGE_LINE:
                            line_baud_ = host_serial_unpack_line(msg.data).baud;
                            if (line_baud_ == 0) {
                                line_baud_ = 9600;
                            }
                            break;
                            
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
