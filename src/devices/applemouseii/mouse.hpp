#pragma once

#include <cstdint>
#include "computer.hpp"
#include "NClock.hpp"
#include "util/EventTimer.hpp"

#define R_POS_XL        0xC080
#define R_POS_XH        0xC081
#define R_POS_YL        0xC082
#define R_POS_YH        0xC083
#define R_CLP_XL_L      0xC084
#define R_CLP_XL_H      0xC085
#define R_CLP_XH_L      0xC086
#define R_CLP_XH_H      0xC087
#define R_CLP_YL_L      0xC088
#define R_CLP_YL_H      0xC089
#define R_CLP_YH_L      0xC08A
#define R_CLP_YH_H      0xC08B
#define R_STATUS        0xC08E
#define R_MODE          0xC08F

typedef struct {
    union {
        int16_t value;
        struct {
            uint8_t l;
            uint8_t h;
        };
    };
} m_i16;

typedef struct {
    union {
        uint8_t value;
        struct {
            uint8_t reserved0:1;
            uint8_t int_motion:1;
            uint8_t int_button:1;
            uint8_t int_vbl:1;
            uint8_t reserved4:1;
            uint8_t x_y_changed:1; // x/y changed since last read
            uint8_t last_button_down:1;
            uint8_t button_down:1;
        };
    };
} m_status;

typedef struct {
    union {
        uint8_t value;
        struct {
            uint8_t mouse_enabled:1;
            uint8_t int_ena_motion:1;
            uint8_t int_ena_button:1;
            uint8_t int_ena_vbl:1;
            uint8_t reserved4:1;
            uint8_t reserved5:1;
            uint8_t reserved6:1;
            uint8_t reserved7:1;
        };
    };
} m_mode;

#define MOUSE_INT_MASK 0b00001110

struct mouse_state_t: public SlotData {
    uint8_t *rom;
    computer_t *computer;
    NClock *clock;
    EventTimer *event_timer;

    m_i16 x_pos;
    m_i16 y_pos;
    m_i16 x_clamp_low;
    m_i16 x_clamp_high;
    m_i16 y_clamp_low;
    m_i16 y_clamp_high;
    m_status status;
    m_mode mode;

    bool button_last_read;

    uint64_t vbl_cycle;
    uint64_t vbl_offset = 0;
    uint16_t last_x_pos;
    uint16_t last_y_pos;
    int16_t motion_x;
    int16_t motion_y;
};

void init_mouse(computer_t *computer, SlotType_t slot);