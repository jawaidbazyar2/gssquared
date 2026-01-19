#pragma once

#include <cstdint>

const uint8_t adb_ascii_us[256] = {
// Letters (home row order roughly)
    'a', // ADB_A = 0x00,
    's', //ADB_S = 0x01,
    'd', //ADB_D = 0x02,
    'f', //ADB_F = 0x03,
    'h', //ADB_H = 0x04,
    'g', //ADB_G = 0x05,
    'z', //ADB_Z = 0x06,
    'x', //ADB_X = 0x07,
    'c', //ADB_C = 0x08,
    'v', //ADB_V = 0x09,
    0x00, // 0x0A unused
    'b', //ADB_B = 0x0B,
    'q', //ADB_Q = 0x0C,
    'w', //ADB_W = 0x0D,
    'e', //ADB_E = 0x0E,
    'r', //ADB_R = 0x0F,
    'y', //ADB_Y = 0x10,
    't', //ADB_T = 0x11,
    // Numbers
    '1', //ADB_1 = 0x12,
    '2', //ADB_2 = 0x13,
    '3', //ADB_3 = 0x14,
    '4', //ADB_4 = 0x15,
    '6', //ADB_6 = 0x16,
    '5', //ADB_5 = 0x17,
    '=', //ADB_EQUAL = 0x18,
    '9', //ADB_9 = 0x19,
    '7', //ADB_7 = 0x1A,
    '-', //ADB_MINUS = 0x1B,
    '8', //ADB_8 = 0x1C,
    '0', //ADB_0 = 0x1D,
    // More punctuation and letters
    ']', //ADB_RIGHT_BRACKET = 0x1E,
    'o', //ADB_O = 0x1F,
    'u', //ADB_U = 0x20,
    '[', //ADB_LEFT_BRACKET = 0x21,
    'i', //ADB_I = 0x22,
    'p', //ADB_P = 0x23,
    0x0D, //ADB_RETURN = 0x24,
    'l', //ADB_L = 0x25,
    'j', //ADB_J = 0x26,
    '\'', //ADB_QUOTE = 0x27,
    'k', //ADB_K = 0x28,
    ';', //ADB_SEMICOLON = 0x29,
    '\\', // ADB_BACKSLASH = 0x2A, no
    ',', //ADB_COMMA = 0x2B,
    '/', //ADB_SLASH = 0x2C,
    'n', //ADB_N = 0x2D,
    'm', //ADB_M = 0x2E,
    '.', //ADB_PERIOD = 0x2F,
    // Special keys
    0x09, //ADB_TAB = 0x30,
    ' ', //ADB_SPACE = 0x31,
    '`', //ADB_GRAVE = 0x32,  // ` / ~
    0x7F, //ADB_DELETE = 0x33,
    0x00, // 0x34 unused
    0x1B, //ADB_ESCAPE = 0x35,
    0x00, //ADB_CONTROL = 0x36,
    0x00, //ADB_COMMAND = 0x37,
    0x00, //ADB_LEFT_SHIFT = 0x38,
    0x00, //ADB_CAPS_LOCK = 0x39,
    0x00, //ADB_OPTION = 0x3A,
    0x08, //ADB_LEFT_ARROW = 0x3B,
    0x15, //ADB_RIGHT_ARROW = 0x3C,
    0x0A, //ADB_DOWN_ARROW = 0x3D,
    0x0B, //ADB_UP_ARROW = 0x3E,
    0xFF, // 0x3F unused
    0xFF, // 0x40 unused
    '.', // ADB_KEYPAD_PERIOD = 0x41,
    0xFF, // 0x42 unused
    '*', //ADB_KEYPAD_MULTIPLY = 0x43,
    0xFF, // 0x44
    '+', // ADB_KEYPAD_PLUS = 0x45,
    0xFF, // 0x46
    0xFF, // 0x47
    0xFF, // 0x48
    0xFF, // 0x49
    0xFF, // 0x4A,
    '/', // ADB_KEYPAD_DIVIDE = 0x4B,
    0x0D, // ADB_KEYPAD_ENTER = 0x4C,
    0xFF, // 0x4D
    '-', // ADB_KEYPAD_MINUS = 0x4E,
    0xFF, // 0x4F
    0xFF, // 0x50
    '=', // ADB_KEYPAD_EQUAL = 0x51,
    '0', // ADB_KEYPAD_0 = 0x52,
    '1', // ADB_KEYPAD_1 = 0x53,
    '2', // ADB_KEYPAD_2 = 0x54,
    '3', // ADB_KEYPAD_3 = 0x55,
    '4', // ADB_KEYPAD_4 = 0x56,
    '5', // ADB_KEYPAD_5 = 0x57,
    '6', // ADB_KEYPAD_6 = 0x58,
    '7', // ADB_KEYPAD_7 = 0x59,
    0xFF, // 0x5A
    '8', // ADB_KEYPAD_8 = 0x5B,
    '9', // ADB_KEYPAD_9 = 0x5C,    
};