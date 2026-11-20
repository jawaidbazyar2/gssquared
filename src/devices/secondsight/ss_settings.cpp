#include "ss_settings.hpp"
#include "util/SystemSettings.hpp"
#include <string>

namespace {

const char *const DEVICE = "second_sight";
const char *const FIRMWARE[] = {"1.4", "1.5", "2.0"};
const char *const MONITOR[] = {"rgb", "vga_crt", "vga_lcd"};
const char *const VIEW[] = {"apple", "card", "both"};

// The index of the stored name in names, or fallback when it is not one of them.
template <size_t N>
int stored_choice(const char *key, const char *const (&names)[N], int fallback) {
    const std::string value = SystemSettings::instance().device_setting(DEVICE, key, names[fallback]);
    for (size_t i = 0; i < N; ++i)
        if (value == names[i]) return int(i);
    return fallback;
}

void store(const char *key, const char *value) {
    SystemSettings::instance().set_device_setting(DEVICE, key, value);
}

} // namespace

void SSSettings::load() {
    const auto &settings = SystemSettings::instance();
    firmware = stored_choice("firmware", FIRMWARE, FW_14);
    lattice_v2 = settings.device_setting(DEVICE, "lattice_chip", "original") == "v2.0";
    int0_pullup = settings.device_setting(DEVICE, "int0_pullup", "false") == "true";
    monitor = stored_choice("monitor", MONITOR, MONITOR_RGB);
    view = stored_choice("view", VIEW, VIEW_BOTH);
}

void SSSettings::set_firmware(int value) {
    if (value < 0 || value >= FW_COUNT) return;
    firmware = value;
    store("firmware", FIRMWARE[value]);
}

void SSSettings::set_lattice_v2(bool value) {
    lattice_v2 = value;
    store("lattice_chip", value ? "v2.0" : "original");
}

void SSSettings::set_int0_pullup(bool value) {
    int0_pullup = value;
    store("int0_pullup", value ? "true" : "false");
}

void SSSettings::set_monitor(int value) {
    if (value < 0 || value >= MONITOR_COUNT) return;
    monitor = value;
    store("monitor", MONITOR[value]);
}

void SSSettings::set_view(int value) {
    if (value < 0 || value >= VIEW_COUNT) return;
    view = value;
    store("view", VIEW[value]);
}

const char *SSSettings::rom_filename() const {
    switch (firmware) {
    case FW_15: return "ROM_CBFC.bin";
    case FW_20: return "ROM_3277.bin";
    default: return "ROM_BEEF.bin";
    }
}
