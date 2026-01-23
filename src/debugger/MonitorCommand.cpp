#include "debugger/MonitorCommand.hpp"

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

/**
  * create a MonitorCommand from a string.
  *
  * The command string is parsed into a vector of mon_node_entry_t objects.
  *
  * "string" -> MON_NODE_TYPE_STRING
  * "1234" -> MON_NODE_TYPE_ADDRESS
  * "1234:" -> MON_NODE_TYPE_ADDRESS with implicit SET command
  * "1234.5678" -> MON_NODE_TYPE_RANGE
  * "55" -> MON_NODE_TYPE_VALUE (byte value in hex)
  * bare_string -> MON_NODE_TYPE_COMMAND
  * nodes are separated by spaces; except that the ADDRESS with implicit SET can be separated from next value by a colon.
  *
  * @param command the command string
  * @return the MonitorCommand
  */

void MonitorCommand::print() const {
    
    for (const auto &node : nodes) {
        std::cout << "node type:" << node.type << " ";
        switch (node.type) {
            case MON_NODE_TYPE_STRING:
                std::cout << "  string: " << node.val_string << std::endl;
                break;
            case MON_NODE_TYPE_NUMBER:
                std::cout << "  number: " << std::hex << node.val_number << std::dec << std::endl;
                break;
            case MON_NODE_TYPE_ADDRESS_SET:
                std::cout << "  address: " << std::hex << node.val_address << std::dec << " (SET)" << std::endl;
                break;
            case MON_NODE_TYPE_RANGE:
                std::cout << "  range: " << std::hex << node.val_range.lo << " - " << node.val_range.hi << std::dec << std::endl;
                break;
            case MON_NODE_TYPE_COMMAND:
                std::cout << "  command: " << node.val_string << "(" << node.val_cmd << ")" << std::endl;
                break;
        }
    }
}

mon_cmd_type_t MonitorCommand::lookup_cmd(const std::string &cmd) {
    if (cmd == "set") return MON_CMD_SET;
    if (cmd == "load") return MON_CMD_LOAD;
    if (cmd == "save") return MON_CMD_SAVE;
    if (cmd == "move") return MON_CMD_MOVE;
    if (cmd == "verify") return MON_CMD_VERIFY;
    if (cmd == "watch") return MON_CMD_WATCH;
    if (cmd == "nowatch") return MON_CMD_NOWATCH;
    if (cmd == "help") return MON_CMD_HELP;
    if (cmd == "bp") return MON_CMD_BP;
    if (cmd == "nobp") return MON_CMD_NOBP;
    if (cmd == "list") return MON_CMD_LIST;
    if (cmd == "l") return MON_CMD_LIST;
    if (cmd == "map") return MON_CMD_MAP;
    if (cmd == "debug") return MON_CMD_DEBUG;
    if (cmd == "nodebug") return MON_CMD_NODEBUG;
    if (cmd == "sload") return MON_CMD_SLOAD;
    if (cmd == "sclear") return MON_CMD_SCLEAR;
    if (cmd == "slookup") return MON_CMD_SLOOKUP;
    return MON_CMD_UNKNOWN;
}

MonitorCommand::MonitorCommand(const std::string &command) : command(command) {
    std::istringstream iss(command);
    std::vector<std::string> tokens;
    std::string token;
    
    // Split by spaces, but handle colon-separated addresses specially
    while (iss >> token) {
        // Check if token contains a colon (address with implicit SET)
        size_t colon_pos = token.find(':');
        if (colon_pos != std::string::npos && colon_pos < token.length() - 1) {
            // Split at colon
            std::string addr_part = token.substr(0, colon_pos + 1);
            std::string value_part = token.substr(colon_pos + 1);
            tokens.push_back(addr_part);
            tokens.push_back(value_part);
        }
        // Check if token ends with 'l' and treat prior part as address
        else if (!token.empty() && token.back() == 'l') {
            std::string addr_part = token.substr(0, token.length() - 1);
            tokens.push_back("l");
            tokens.push_back(addr_part);
        }
        else {
            tokens.push_back(token);
        }
    }
    
    std::cout << "tokens: ";
    for (const auto &t : tokens) {
        std::cout << t << " ";
    }
    std::cout << std::endl;

    // Parse each token
    for (const std::string& t : tokens) {
        mon_node_entry_t node = {};
        
        if (t.empty()) continue;
        
        // Check if it's a quoted string
        if (t.front() == '"' && t.back() == '"') {
            node.type = MON_NODE_TYPE_STRING;
            node.val_string = t.substr(1, t.length() - 2);
        }
        // Check if it's an address with implicit SET (ends with colon)
        else if (t.back() == ':') {
            node.type = MON_NODE_TYPE_ADDRESS_SET;
            std::string addr_str = t.substr(0, t.length() - 1);
            node.val_address = static_cast<uint32_t>(std::stoul(addr_str, nullptr, 16));
        }
        // Check if it's a range (contains dot)
        else if (t.find('.') != std::string::npos) {
            node.type = MON_NODE_TYPE_RANGE;
            size_t dot_pos = t.find('.');
            std::string lo_str = t.substr(0, dot_pos);
            std::string hi_str = t.substr(dot_pos + 1);
            node.val_range.lo = static_cast<uint32_t>(std::stoul(lo_str, nullptr, 16));
            node.val_range.hi = static_cast<uint32_t>(std::stoul(hi_str, nullptr, 16));
        }
        // Check if it's all hex digits (address or value)
        else if (std::all_of(t.begin(), t.end(), [](char c) { 
            return std::isxdigit(c); 
        })) {
            uint32_t val = static_cast<uint32_t>(std::stoul(t, nullptr, 16));

            node.type = MON_NODE_TYPE_NUMBER;
            node.val_number = val; 
        }
        // Otherwise it's a command word
        else {
            node.type = MON_NODE_TYPE_COMMAND;
            std::string lower_t = t;
            std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
            node.val_string = lower_t;
            node.val_cmd = lookup_cmd(lower_t);
            // Could parse specific command types if needed
        }
        
        nodes.push_back(node);
    }
}
