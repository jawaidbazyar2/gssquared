#pragma once

#include "ADB_Device.hpp"

/*
* Mouse Register 0
  * Bit 15: Button Pressed
  * Bit 14: Moved Up
  * Bit 13-8: Y move value
  * Bit 7: always 1
  * Bit 6: Moved right
  * Bit 5-0: X move value
*/

class ADB_Mouse : public ADB_Device
{
    private:
        static constexpr float MOUSE_MOTION_SCALE_SLOW = 0.200f;
        static constexpr float MOUSE_MOTION_SCALE_FAST = 0.400f;
        uint8_t button_0_down = 0x80; // default to button up
        uint8_t button_1_down = 0x80;
        bool has_data = false;
        uint8_t handler = 1; // default to low resolution mouse

    public:
    ADB_Mouse(uint8_t id = 0x03) : ADB_Device(id) {
        registers[0].size = 2;
        registers[0].data[0] = 0x00;
        registers[0].data[1] = 0x00;
    }

    void reset(uint8_t cmd, uint8_t reg) override { }
    void flush(uint8_t cmd, uint8_t reg) override { }
    void listen(uint8_t command, uint8_t reg, ADB_Register &msg) override {
        printf("MS> Listen: command: %02X, reg: %02X, msg: %02X %02X\n", command, reg, msg.data[0], msg.data[1]);
        if (reg == 3) { // 
            /** Register 3
             * Bit 15: reserved, must be 0. (byte 0)
             * Bit 14: exceptional event.
             * Bit 13: SR enable
             * Bit 12: Reserved, must be 0.
             * Bit 11-8: Device address.
             * Bit 7-0: Device handler. (byte 1)
             */
            registers[3] = msg;
            id = msg.data[0] & 0x0F; // change device address
            handler = msg.data[1] & 0x0F;  // (1= lo res mouse vs 2= hi res mouse)
            printf("MS> New address: %02X, handler: %02X\n", id, handler);
        }
    }
    ADB_Register talk(uint8_t command, uint8_t reg) override {

        ADB_Register reg_result = {0};
        if (has_data) {
            reg_result = registers[0];
            has_data = false;
        }
        return reg_result;
    }

    void update_button_down() {
        registers[0].data[0] = button_1_down | (registers[0].data[0] & 0x7F);
        registers[0].data[1] = button_0_down | (registers[0].data[1] & 0x7F);
    }

    bool process_event(SDL_Event &event) override {
        bool status = false;
        /* Track mouse button up and down events. In ADB Land, the button up/down status 
           is sent back to host via reads of register 0. Presumably in a case where there is only button change,
           the host sees relx,y of 0,0.
           And, SDL3 gives us a final relative X,Y of 0,0 when the mouse stops moving.
           So this works out. Since this is implicit, I have an uncharacteristic comment here.
        */
        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                button_0_down = 0x00;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                button_1_down = 0x00;
            }
            //button_0_down = 0x00;
            has_data = true;
            registers[0].data[0] = 0;
            registers[0].data[1] = 0;
            update_button_down();
            status = true;
            
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            if (event.button.button == SDL_BUTTON_LEFT) {
                button_0_down = 0x80;
            } else if (event.button.button == SDL_BUTTON_RIGHT) {
                button_1_down = 0x80;
            }
            //button_0_down = 0x80;
            has_data = true;
            registers[0].data[0] = 0;
            registers[0].data[1] = 0;
            update_button_down();
            status = true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // TODO: determine if we need to do any kind of scaling
            // it's also possible we need to accumulate motion values 
            // until the register is actually read.
            // alternatively, to buffer them.
            float scale = handler == 1 ? MOUSE_MOTION_SCALE_SLOW : MOUSE_MOTION_SCALE_FAST;
            int xrel = (int)(event.motion.xrel * scale);
            int yrel = (int)(event.motion.yrel * scale);
            
            // if there was any motion, minimum motion after scale is 1.
            if (xrel == 0 && event.motion.xrel > 0) xrel = 1;
            if (xrel == 0 && event.motion.xrel < 0) xrel = -1;
            if (yrel == 0 && event.motion.yrel > 0) yrel = 1;
            if (yrel == 0 && event.motion.yrel < 0) yrel = -1;

            bool moved_right, moved_up;

            if (xrel < 0) {
                moved_right = true;
            } else {
                moved_right = false;
            }

            if (yrel < 0) {
                moved_up = true;
            } else {
                moved_up = false;
            }

            // set motion bits, and make positive if negative
            /* if (xrel > 0) moved_right = 1;
            else xrel = -xrel;
            if (yrel < 0) { moved_up = 1; yrel = -yrel; } */

            // clamp to 0 .. 63
            if (xrel > 63) xrel = 63;
            if (yrel > 63) yrel = 63;

            // negative values are 2's complement.
            registers[0].size = 2;
            registers[0].data[0] = (uint8_t)(xrel & 0x3F) | (moved_right ? 0x40 : 0x00);
            registers[0].data[1] = (uint8_t)(yrel & 0x3F) | (moved_up ? 0x40 : 0x00);
            update_button_down();
            has_data = true;
            status = true;

            //printf("MS> Mouse motion: x: %f, y: %f, xrel_abs: %d, yrel_abs: %d, moved_right: %d, moved_up: %d data0: %02X, data1: %02X\n", event.motion.x, event.motion.y, xrel, yrel, moved_right, moved_up, registers[0].data[0], registers[0].data[1]);
        }
        if (status) print_registers();
        return status;
    }
};
