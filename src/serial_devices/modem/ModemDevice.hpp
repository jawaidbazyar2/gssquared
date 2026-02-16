#pragma once

#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm> // Required for remove_if

#include "serial_devices/SerialDevice.hpp"
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
        std::string command_buffer;
        NET_StreamSocket *socket;
        std::string remote_host;
        int remote_port;
        bool command_echo = true;
        
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
                q_dev.send(msg);
            }
        }

        void send_ok_response() {
            send_response("OK\r\n");
        }

        void send_error_response() {
            send_response("ERROR\r\n");
        }

        void send_no_carrier_response() {
            send_response("NO CARRIER\r\n");
        }

        void process_command() {
            // Normalize command to uppercase
            std::string cmd = command_buffer;
            for (auto &c : cmd) {
                c = toupper(c);
            }

            printf("ModemDevice: Processing command: %s\n", cmd.c_str());

            // Remove AT prefix if present
            if (cmd.length() >= 2 && cmd.substr(0, 2) == "AT") {
                cmd = cmd.substr(2);
            }

            if (cmd.empty()) {
                // Just "AT" - acknowledge
                send_ok_response();
            } else if (cmd[0] == 'D') {
                // ATD - Dial command
                std::string address = cmd.substr(1);
                
                // Skip optional T (tone) or P (pulse) dialing mode
                if (!address.empty() && (address[0] == 'T' || address[0] == 'P')) {
                    address = address.substr(1);
                }
                
                // Remove any spaces
                address.erase(remove_if(address.begin(), address.end(), ::isspace), address.end());
                
                if (address.empty()) {
                    send_error_response();
                } else {
                    dial(address);
                }
            } else if (cmd[0] == 'H') {
                // ATH - Hang up
                hangup();
                send_ok_response();
            } else if (cmd[0] == 'Z') {
                // ATZ - Reset modem
                hangup();
                send_ok_response();
            } else if (cmd[0] == 'E') {
                // ATE0/ATE1 - Echo off/on (just acknowledge)
                if (cmd[1] == '0') {
                    // Echo off
                    command_echo = false;
                    send_ok_response();
                } else if (cmd[1] == '1') {
                    // Echo on
                    command_echo = true;
                    send_ok_response();
                } else {
                    send_error_response();
                }
                
            } else if (cmd[0] == 'V') {
                // ATV0/ATV1 - Verbose off/on (just acknowledge)
                send_ok_response();
            } else if (cmd[0] == 'Q') {
                // ATQ0/ATQ1 - Result codes on/off (just acknowledge)
                send_ok_response();
            } else {
                // Unknown command
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

            // Connection successful
            state = STATE_ONLINE;
            telnet_state = TELNET_DATA;  // Reset telnet protocol state
            tcp_buffer.clear();  // Clear any buffered data
            
            // Negotiate binary mode in both directions for 8-bit transparency
            send_telnet_response(WILL, TELOPT_BINARY);  // We will send binary
            send_telnet_response(DO, TELOPT_BINARY);    // Please send us binary
            
            send_response("CONNECT 9600\r\n");
            printf("ModemDevice: Connected!\n");
        }

        void hangup() {
            if (socket) {
                printf("ModemDevice: Hanging up\n");
                NET_DestroyStreamSocket(socket);
                socket = nullptr;
            }
            state = STATE_COMMAND;
            telnet_state = TELNET_DATA;  // Reset telnet protocol state
            tcp_buffer.clear();  // Clear any buffered data
        }

        void handle_escape_sequence() {
            printf("ModemDevice: Escape sequence detected, returning to command mode\n");
            state = STATE_COMMAND;
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
        ModemDevice(const char *name, const char *port_id) : SerialDevice("ModemDevice", port_id), 
                       state(STATE_COMMAND),
                       socket(nullptr),
                       remote_port(23),
                       last_char_time(0),
                       escape_count(0),
                       telnet_state(TELNET_DATA),
                       telnet_command(0) {
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
                            
                        case MESSAGE_DATA: {
                            uint8_t byte = static_cast<uint8_t>(msg.data);
                            uint64_t current_time = SDL_GetTicks();
                            
                            if (state == STATE_COMMAND) {
                                if (command_echo) q_dev.send({MESSAGE_DATA, byte});

                                // Command mode - process AT commands
                                if (byte == '\r' || byte == '\n') {
                                    if (!command_buffer.empty()) {
                                        process_command();
                                    }
                                } else if (byte == 8 || byte == 127) {
                                    // Backspace or DEL
                                    if (!command_buffer.empty()) {
                                        command_buffer.pop_back();
                                    }
                                } else if (byte >= 32 && byte < 127) {
                                    // Printable character
                                    command_buffer += static_cast<char>(byte);
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
                                                state = STATE_ESCAPE;
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
                                    state = STATE_ONLINE;
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
