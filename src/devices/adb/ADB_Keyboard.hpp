#pragma once

#include "ADB_Device.hpp"

struct adb_mod_key_t {
    union {
        uint8_t value;
        struct {
            uint8_t shift: 1;
            uint8_t ctrl: 1;
            uint8_t caps: 1;
            uint8_t repeat: 1;
            uint8_t keypad: 1;
            uint8_t updated: 1;
            uint8_t closed: 1;
            uint8_t open: 1;
        };
    };
};

enum key_status_t {
    KEY_STATUS_UP = 0,
    KEY_STATUS_DOWN = 1,
};

struct key_code_t {
    uint8_t keycode;
    adb_mod_key_t keymods;
};

struct key_event_t {
    uint8_t keycode;
    key_status_t status;
};

enum adb_keycode_t {
    // Letters (home row order roughly)
    ADB_A = 0x00,
    ADB_S = 0x01,
    ADB_D = 0x02,
    ADB_F = 0x03,
    ADB_H = 0x04,
    ADB_G = 0x05,
    ADB_Z = 0x06,
    ADB_X = 0x07,
    ADB_C = 0x08,
    ADB_V = 0x09,
    // 0x0A unused
    ADB_B = 0x0B,
    ADB_Q = 0x0C,
    ADB_W = 0x0D,
    ADB_E = 0x0E,
    ADB_R = 0x0F,
    ADB_Y = 0x10,
    ADB_T = 0x11,
    // Numbers
    ADB_1 = 0x12,
    ADB_2 = 0x13,
    ADB_3 = 0x14,
    ADB_4 = 0x15,
    ADB_6 = 0x16,
    ADB_5 = 0x17,
    ADB_EQUAL = 0x18,
    ADB_9 = 0x19,
    ADB_7 = 0x1A,
    ADB_MINUS = 0x1B,
    ADB_8 = 0x1C,
    ADB_0 = 0x1D,
    // More punctuation and letters
    ADB_RIGHT_BRACKET = 0x1E,
    ADB_O = 0x1F,
    ADB_U = 0x20,
    ADB_LEFT_BRACKET = 0x21,
    ADB_I = 0x22,
    ADB_P = 0x23,
    ADB_RETURN = 0x24,
    ADB_L = 0x25,
    ADB_J = 0x26,
    ADB_QUOTE = 0x27,
    ADB_K = 0x28,
    ADB_SEMICOLON = 0x29,
    ADB_BACKSLASH = 0x2A, // no
    ADB_COMMA = 0x2B,
    ADB_SLASH = 0x2C,
    ADB_N = 0x2D,
    ADB_M = 0x2E,
    ADB_PERIOD = 0x2F,
    // Special keys
    ADB_TAB = 0x30,
    ADB_SPACE = 0x31,
    ADB_GRAVE = 0x32,  // ` / ~
    ADB_DELETE = 0x33,
    // 0x34 unused
    ADB_ESCAPE = 0x35,
    ADB_CONTROL = 0x36,
    ADB_COMMAND = 0x37,
    ADB_LEFT_SHIFT = 0x38,
    ADB_CAPS_LOCK = 0x39,
    ADB_OPTION = 0x3A,
    ADB_LEFT_ARROW = 0x3B,
    ADB_RIGHT_ARROW = 0x3C,
    ADB_DOWN_ARROW = 0x3D,
    ADB_UP_ARROW = 0x3E,
    // 0x3F - 0x40 unused
    // Keypad
    ADB_KEYPAD_PERIOD = 0x41,
    // 0x42 unused
    ADB_KEYPAD_MULTIPLY = 0x43,
    // 0x44 unused
    ADB_KEYPAD_PLUS = 0x45,
    // 0x46 unused
    ADB_KEYPAD_CLEAR = 0x47,
    // 0x48 - 0x4A unused
    ADB_KEYPAD_DIVIDE = 0x4B,
    ADB_KEYPAD_ENTER = 0x4C,
    // 0x4D unused
    ADB_KEYPAD_MINUS = 0x4E,
    // 0x4F - 0x50 unused
    ADB_KEYPAD_EQUAL = 0x51,
    ADB_KEYPAD_0 = 0x52,
    ADB_KEYPAD_1 = 0x53,
    ADB_KEYPAD_2 = 0x54,
    ADB_KEYPAD_3 = 0x55,
    ADB_KEYPAD_4 = 0x56,
    ADB_KEYPAD_5 = 0x57,
    ADB_KEYPAD_6 = 0x58,
    ADB_KEYPAD_7 = 0x59,
    // 0x5A unused
    ADB_KEYPAD_8 = 0x5B,
    ADB_KEYPAD_9 = 0x5C,
    // 0x5D - 0x7A unused
    ADB_RIGHT_SHIFT = 0x7B,
    // Power key is special: 0x7F7F (two-byte code)
};

class ADB_Keyboard : public ADB_Device
{
    private:
    key_event_t keyqueue[16];
    uint32_t index_in = 0;
    uint32_t index_out = 0;
    uint32_t count = 0;

    const uint8_t sdl_to_adb_key_map[SDL_SCANCODE_COUNT] = {
        0xFF,
        0xFF,
        0xFF,
        0xFF,
        ADB_A, // SDL_SCANCODE_A = 4,
        ADB_B, // SDL_SCANCODE_B = 5,
        ADB_C, // SDL_SCANCODE_C = 6,
        ADB_D, // SDL_SCANCODE_D = 7,
        ADB_E, // SDL_SCANCODE_E = 8,
        ADB_F, // SDL_SCANCODE_F = 9,
        ADB_G, // SDL_SCANCODE_G = 10,
        ADB_H, // SDL_SCANCODE_H = 11,
        ADB_I, // SDL_SCANCODE_I = 12,
        ADB_J, // SDL_SCANCODE_J = 13,
        ADB_K, // SDL_SCANCODE_K = 14,
        ADB_L, // SDL_SCANCODE_L = 15,
        ADB_M, // SDL_SCANCODE_M = 16,
        ADB_N, // SDL_SCANCODE_N = 17,
        ADB_O, // SDL_SCANCODE_O = 18,
        ADB_P, // SDL_SCANCODE_P = 19,
        ADB_Q, // SDL_SCANCODE_Q = 20,
        ADB_R, // SDL_SCANCODE_R = 21,
        ADB_S, // SDL_SCANCODE_S = 22,
        ADB_T, // SDL_SCANCODE_T = 23,
        ADB_U, // SDL_SCANCODE_U = 24,
        ADB_V, // SDL_SCANCODE_V = 25,
        ADB_W, // SDL_SCANCODE_W = 26,
        ADB_X, // SDL_SCANCODE_X = 27,
        ADB_Y, // SDL_SCANCODE_Y = 28,
        ADB_Z, // SDL_SCANCODE_Z = 29,
        ADB_1, // SDL_SCANCODE_1 = 30,
        ADB_2, // SDL_SCANCODE_2 = 31,
        ADB_3, // SDL_SCANCODE_3 = 32,
        ADB_4, // SDL_SCANCODE_4 = 33,
        ADB_5, // SDL_SCANCODE_5 = 34,
        ADB_6, // SDL_SCANCODE_6 = 35,
        ADB_7, // SDL_SCANCODE_7 = 36,
        ADB_8, // SDL_SCANCODE_8 = 37,
        ADB_9, // SDL_SCANCODE_9 = 38,
        ADB_0, // SDL_SCANCODE_0 = 39,
        ADB_RETURN, // SDL_SCANCODE_RETURN = 40,
        ADB_ESCAPE, // SDL_SCANCODE_ESCAPE = 41,
        ADB_DELETE, // SDL_SCANCODE_BACKSPACE = 42,
        ADB_TAB, // SDL_SCANCODE_TAB = 43,
        ADB_SPACE, // SDL_SCANCODE_SPACE = 44,
        ADB_MINUS, // SDL_SCANCODE_MINUS = 45,
        ADB_EQUAL, // SDL_SCANCODE_EQUALS = 46,
        ADB_LEFT_BRACKET,  // SDL_SCANCODE_LEFTBRACKET = 47,
        ADB_RIGHT_BRACKET, // SDL_SCANCODE_RIGHTBRACKET = 48,
        ADB_BACKSLASH, // SDL_SCANCODE_BACKSLASH = 49,
        0x7F,
        ADB_SEMICOLON,
        ADB_QUOTE,
        ADB_GRAVE, // SDL_SCANCODE_GRAVE = 53
        ADB_COMMA, // SDL_SCANCODE_COMMA = 50,
        ADB_PERIOD, // SDL_SCANCODE_PERIOD = 51,
        ADB_SLASH, // SDL_SCANCODE_SLASH = 52,
        ADB_CAPS_LOCK, // SDL_SCANCODE_CAPSLOCK, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        ADB_RIGHT_ARROW, 
        ADB_LEFT_ARROW, 
        ADB_DOWN_ARROW, 
        ADB_UP_ARROW, 
        0xFF, 
        ADB_KEYPAD_DIVIDE, 
        ADB_KEYPAD_MULTIPLY, 
        ADB_KEYPAD_MINUS, 
        ADB_KEYPAD_PLUS, 
        ADB_KEYPAD_ENTER, 
        ADB_KEYPAD_1, 
        ADB_KEYPAD_2, 
        ADB_KEYPAD_3, 
        ADB_KEYPAD_4, 
        ADB_KEYPAD_5, 
        ADB_KEYPAD_6, 
        ADB_KEYPAD_7, 
        ADB_KEYPAD_8, 
        ADB_KEYPAD_9, 
        ADB_KEYPAD_0, 
        ADB_KEYPAD_PERIOD, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF, 
        0xFF,
        ADB_CONTROL, // SDL_SCANCODE_LCTRL = 224,
        ADB_LEFT_SHIFT, // SDL_SCANCODE_LSHIFT = 225,
        ADB_COMMAND, // SDL_SCANCODE_LGUI = 226,
        ADB_OPTION, // SDL_SCANCODE_LALT = 227,
        ADB_CONTROL, // SDL_SCANCODE_RCTRL = 228,
        ADB_LEFT_SHIFT, // SDL_SCANCODE_RSHIFT = 229,
        ADB_COMMAND, // SDL_SCANCODE_RGUI = 230,
        ADB_OPTION, // SDL_SCANCODE_RALT = 231,
    };

    void print_key_buffer() {
        printf("02::Key buffer: ");
        uint32_t j = index_out;
        for (int i = 0; i < count; i++) {
            printf("%s %02X  ", keyqueue[j].status == KEY_STATUS_UP ? "↑" : "↓", keyqueue[j].keycode);
            j = (j + 1) % 16;
        }
        printf("\n");
    }

    void enqueue_key(key_event_t key) {
        if (count == 16) return;
        keyqueue[index_in] = key;
        index_in = (index_in + 1) % 16;
        count++;
        //print_key_buffer();
    }

    void dequeue_key(key_event_t *key) {
        if (count == 0) return;
        *key = keyqueue[index_out];
        index_out = (index_out + 1) % 16;
        count--;        
    }

    public:
    ADB_Keyboard(uint8_t id = 0x02) : ADB_Device(id) {
        registers[0].size = 2;
        registers[0].data[0] = 0xFF; // hi byte
        registers[0].data[1] = 0xFF; // lo byte
        
        registers[1].size = 2;
        registers[1].data[0] = 0x00;
        registers[1].data[1] = 0x00;
        
        registers[2].size = 2;
        registers[2].data[0] = 0x00;
        registers[2].data[1] = 0x00;

        /** Register 3
            * Bit 15: reserved, must be 0.
            * Bit 14: exceptional event.
            * Bit 13: SR enable
            * Bit 12: Reserved, must be 0.
            * Bit 11-8: Device address.
            * Bit 7-0: Device handler. */
    }

    void reset(uint8_t cmd, uint8_t reg) override { }

    void flush(uint8_t cmd, uint8_t reg) override { }

    void listen(uint8_t command, uint8_t reg, ADB_Register &msg) override { 
        //printf("KB> Listen: command: %02X, reg: %02X, msg: %02X %02X\n", command, reg, msg.data[0], msg.data[1]);
        if (reg == 3) {
            registers[3] = msg;
            id = msg.data[1] & 0x0F; // change device address
        }
    }

    ADB_Register talk(uint8_t command, uint8_t reg) override {
        ADB_Register reg_result = {};
        reg_result.size = registers[reg].size;
        for (int i = 0; i < registers[reg].size; i++) {
            reg_result.data[i] = registers[reg].data[i];
        }
        registers[reg].data[0] = 0xFF; // clear reg0
        registers[reg].data[1] = 0xFF;
        return reg_result;
    }

    bool process_event(SDL_Event &event) override {
        // register 0 is keyboard data.
        uint8_t k1_released = 0;
        uint8_t k2_released = 0;
        bool status = false;

        if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
            const char *updown = (event.type == SDL_EVENT_KEY_DOWN) ? "down" : "up";    
            printf("02::Key %s: %08X\n", updown, event.key.key);
            k1_released = 0;
            map_sdl_to_adb_key_enqueue(event);
            status = true;
        }

        // if there is something in the key queue, and room in register 0,
        // dequeue the key and put it into register 0.
        // the LSB (byte) first.
        if (registers[0].data[0] == 0xFF && count > 0) {
            key_event_t key;
            dequeue_key(&key);
            uint8_t code = ((key.status == KEY_STATUS_UP) ? 0x80 : 0x00) | key.keycode;
            registers[0].data[0] = code;
        }
        // and now the MSB byte.
        if (registers[0].data[1] == 0xFF && count > 0) {
            key_event_t key;
            dequeue_key(&key);
            uint8_t code = ((key.status == KEY_STATUS_UP) ? 0x80 : 0x00) | key.keycode;
            registers[0].data[1] = code;
        }
        //if (status) print_registers();

        return status;
    }

    void map_sdl_to_adb_key_enqueue(SDL_Event &event) {
        key_event_t key = {};
        uint32_t scancode = event.key.scancode;
        scancode &= (SDL_SCANCODE_COUNT-1);
        uint8_t scancode_mapped = sdl_to_adb_key_map[scancode];
        if (scancode_mapped == 0xFF) return; // unmapped, ignore
        key.keycode = scancode_mapped;
        key.status = (event.type == SDL_EVENT_KEY_DOWN) ? KEY_STATUS_DOWN : KEY_STATUS_UP;
        
/*         key.keymods.value = 0;
        key.keymods.ctrl = (SDL_KMOD_CTRL & event.key.mod) ? 1 : 0;
        key.keymods.shift = (SDL_KMOD_SHIFT & event.key.mod) ? 1 : 0;
        key.keymods.open = (SDL_KMOD_ALT & event.key.mod) ? 1 : 0;
        key.keymods.closed = (SDL_KMOD_GUI & event.key.mod) ? 1 : 0;
        key.keymods.caps = (SDL_KMOD_CAPS & event.key.mod) ? 1 : 0; */
        enqueue_key(key);
    }
};