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
        uint8_t button_down = 0x00;
        bool has_data = false;

    public:
    ADB_Mouse(uint8_t id = 0x03) : ADB_Device(id) {
        registers[0].size = 2;
        registers[0].data[0] = 0x00;
        registers[0].data[1] = 0x00;
    }

    void reset(uint8_t cmd, uint8_t reg) override { }
    void flush(uint8_t cmd, uint8_t reg) override { }
    void listen(uint8_t command, uint8_t reg, ADB_Register &msg) override {
        if (reg == 3) { // 
            /** Register 3
             * Bit 15: reserved, must be 0.
             * Bit 14: exceptional event.
             * Bit 13: SR enable
             * Bit 12: Reserved, must be 0.
             * Bit 11-8: Device address.
             * Bit 7-0: Device handler.
             */
            registers[3] = msg;
            id = msg.data[1] & 0x0F; // change device address
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
        registers[0].data[0] = 0x80 | (registers[0].data[0] & 0x7F);
        registers[0].data[1] = button_down | (registers[0].data[1] & 0x7F);
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
            button_down = 0x80;
            has_data = true;
            update_button_down();
            status = true;
        } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
            button_down = 0x00;
            has_data = true;
            update_button_down();
            status = true;
        }
        if (event.type == SDL_EVENT_MOUSE_MOTION) {
            // TODO: determine if we need to do any kind of scaling
            // it's also possible we need to accumulate motion values 
            // until the register is actually read.
            // alternatively, to buffer them.
            int xrel = (int)event.motion.xrel;
            int yrel = (int)event.motion.yrel;
            
            /* if (xrel == 0 && yrel == 0) {
                status = false;
            } else { */
                //printf("MS> Mouse motion: x: %d, y: %d\n", xrel, yrel);
                bool moved_right = false;
                bool moved_up = false;

                // set motion bits, and make positive if negative
                if (xrel > 0) moved_right = 1;
                else xrel = -xrel;
                if (yrel < 0) { moved_up = 1; yrel = -yrel; }

                // clamp to 0 .. 63
                if (xrel > 63) xrel = 63;
                if (yrel > 63) yrel = 63;

                registers[0].size = 2;
                registers[0].data[0] = ((uint8_t)xrel & 0x3F) | (moved_right ? 0x40 : 0x00);
                registers[0].data[1] = ((uint8_t)yrel & 0x3F) | (moved_up ? 0x40 : 0x00);
                update_button_down();
                has_data = true;
                status = true;
            /* } */
        }
        if (status) print_registers();
        return status;
    }
};
