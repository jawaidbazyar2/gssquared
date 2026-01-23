#pragma once 
#include <string>
#include <cstdint>
#include <vector>

struct mon_range_t {
    uint32_t lo;
    uint32_t hi;
};

enum mon_node_type_t {
    MON_NODE_TYPE_NUMBER, // single address
    MON_NODE_TYPE_ADDRESS_SET, // single address with implicit SET command
    MON_NODE_TYPE_RANGE,   // address range
    MON_NODE_TYPE_COMMAND, // command word
    MON_NODE_TYPE_STRING,  // string (like filename)
};

enum mon_cmd_type_t {
    MON_CMD_UNKNOWN,
    MON_CMD_MOVE,
    MON_CMD_VERIFY,
    MON_CMD_LOAD,
    MON_CMD_SAVE,
    MON_CMD_SET,
    MON_CMD_WATCH,
    MON_CMD_NOWATCH,
    MON_CMD_HELP,
    MON_CMD_BP,
    MON_CMD_NOBP,
    MON_CMD_LIST,
    MON_CMD_MAP,
    MON_CMD_DEBUG,
    MON_CMD_NODEBUG,
    MON_CMD_SLOAD, // load symbol table from file
    MON_CMD_SCLEAR, // clear symbol table
    MON_CMD_SLOOKUP, // lookup symbol by address
};

struct mon_node_entry_t {
    mon_node_type_t type;
    mon_cmd_type_t val_cmd;
    uint32_t val_address;
    mon_range_t val_range;   
    std::string val_string;
    uint32_t val_number;
};

struct MonitorCommand {
    std::vector<mon_node_entry_t> nodes;

    std::string command;
    MonitorCommand(const std::string &command);
    void print() const;
    mon_cmd_type_t lookup_cmd(const std::string &cmd);
};
