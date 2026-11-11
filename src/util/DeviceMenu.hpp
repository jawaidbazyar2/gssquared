#pragma once

#include <functional>
#include <string>
#include <vector>

// The application menu that shows a device's menu.
enum device_menu_place {
    DEVICE_MENU_SETTINGS,
    DEVICE_MENU_DISPLAY,
    NUM_DEVICE_MENU_PLACES
};

// One entry of a menu that a device adds while its machine runs
// (computer_t::register_device_menu). An entry with entries is a submenu. An
// entry with select is a choice: select runs on the main thread when the user
// picks it, and checked gives its checkmark. An entry with neither is a note.
struct device_menu_entry_t {
    std::string label;
    std::function<void()> select;
    std::function<bool()> checked;
    std::vector<device_menu_entry_t> entries;
    // Key shown beside a choice that also has a keyboard shortcut.
    std::string shortcut;
};
