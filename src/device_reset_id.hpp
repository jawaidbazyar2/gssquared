#pragma once

enum device_reset_id {
    RST_SLOT_0 = 0,
    RST_SLOT_1 = 1,
    RST_SLOT_2 = 2,
    RST_SLOT_3 = 3,
    RST_SLOT_4 = 4,
    RST_SLOT_5 = 5,
    RST_SLOT_6 = 6,
    RST_SLOT_7 = 7,
    RST_ID_KEYMICRO = 9,       // ADB Micro
    RST_ID_KEYBOARD = 10,      // II+/IIe Keyboard
    RST_ID_POWERUP = 10,
};

typedef void (*device_reset_handler_t)(void *context);
struct device_reset_handler_s {
    device_reset_handler_t handler;
    void *context;
};
