#pragma once

#include <string>

struct drive_status_t {
    bool is_mounted;
    std::string filename;  // Own the string data to avoid dangling pointers
    bool motor_on;
    int position;
    bool is_modified;
};

