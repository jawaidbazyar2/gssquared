#pragma once

#include "ADB_Device.hpp"
#include "agent/Protocol.hpp"   // AGENT_MOUSEID — used to detect injected events

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
        static constexpr float MOUSE_MOTION_SCALE_SLOW = 0.300f;
        static constexpr float MOUSE_MOTION_SCALE_FAST = 0.600f;
        uint8_t button_0_down = 0x80; // default to button up
        uint8_t button_1_down = 0x80;
        bool has_data = false;
        uint8_t handler = 1; // default to low resolution mouse

        // Pending motion accumulator. Pre-fix, process_event() wrote
        // the current frame's xrel/yrel directly into registers[0],
        // which means a second motion event arriving before the IIgs
        // ADB poll consumed the first would clobber it. With the
        // accumulator we sum incoming deltas here and only materialize
        // them into registers[0] when talk() is called (i.e. when the
        // IIgs actually polls). Cleared on read. Stored as int so the
        // accumulator can hold a few hundred SDL pixels without
        // overflowing 16 bits — clamp to ±63 only at the boundary
        // when packing into the 6-bit ADB register field.
        int pending_xrel = 0;
        int pending_yrel = 0;

    public:
    ADB_Mouse(uint8_t id = 0x03) : ADB_Device(id) {
        registers[0].size = 2;
        registers[0].data[0] = 0x00;
        registers[0].data[1] = 0x00;

        registers[1].size = 2;
        registers[1].data[0] = 0x00;
        registers[1].data[1] = 0x00;
        
        registers[2].size = 2;
        registers[2].data[0] = 0x00;
        registers[2].data[1] = 0x00;
    }

    void reset(uint8_t cmd, uint8_t reg) override { }
    void flush(uint8_t cmd, uint8_t reg) override { }
    void listen(uint8_t command, uint8_t reg, ADB_Register &msg) override {
        //printf("MS> Listen: command: %02X, reg: %02X, msg: %02X %02X\n", command, reg, msg.data[0], msg.data[1]);
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
            //printf("MS> New address: %02X, handler: %02X\n", id, handler);
        }
    }
    ADB_Register talk(uint8_t command, uint8_t reg) override {
        ADB_Register reg_result = {0};
        if (!has_data) {
            return reg_result;
        }

        // Pack accumulated motion into registers[0] at poll time.
        //
        // The IIgs ADB mouse register encodes the per-axis delta as
        // SIGNED two's complement in bits 5-0 (sign bit = bit 5),
        // despite the file-top comment suggesting bits 5-0 are an
        // unsigned magnitude with bit 6 as a direction flag. Real-world
        // testing confirms the signed interpretation: sending
        // xrel=+50 to the IIgs ADB mouse register encodes as 0x32,
        // which the IIgs reads as -14 (because bit 5 is set), not +50
        // — and the cursor does in fact move that way. Safe range is
        // therefore [-32, +31]; we clamp at ±31 to avoid the
        // +32-wraps-to-itself-as-negative-32 edge case. The original
        // ±63 clamp was wrong but never bit real users because the
        // 0.3 host-mouse scale factor kept per-poll deltas well under
        // 32 in normal use.
        //
        // Anything past the clamp is *carried* into pending_xrel for
        // the next poll, so long fast moves don't lose ground.
        constexpr int CLAMP = 31;
        int xrel = pending_xrel;
        int yrel = pending_yrel;
        if (xrel > CLAMP) {
            pending_xrel = xrel - CLAMP;
            xrel = CLAMP;
        } else if (xrel < -CLAMP) {
            pending_xrel = xrel + CLAMP;
            xrel = -CLAMP;
        } else {
            pending_xrel = 0;
        }
        if (yrel > CLAMP) {
            pending_yrel = yrel - CLAMP;
            yrel = CLAMP;
        } else if (yrel < -CLAMP) {
            pending_yrel = yrel + CLAMP;
            yrel = -CLAMP;
        } else {
            pending_yrel = 0;
        }

        const bool moved_right = (xrel < 0);
        const bool moved_up    = (yrel < 0);
        registers[0].size = 2;
        registers[0].data[0] = (uint8_t)(xrel & 0x3F) | (moved_right ? 0x40 : 0x00);
        registers[0].data[1] = (uint8_t)(yrel & 0x3F) | (moved_up    ? 0x40 : 0x00);
        update_button_down();

        reg_result = registers[0];
        // Only mark "no more data" if the accumulator is fully drained
        // AND there's no pending button-state change to report. The
        // button-down handlers set has_data=true and zero the motion
        // bits; if we still have residue we want to keep flagging.
        if (pending_xrel == 0 && pending_yrel == 0) {
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
            // Real host SDL events arrive in host pixel deltas and need
            // GSSquared's mouse-feel scale (0.3 SLOW / 0.6 FAST). Agent-
            // injected events arrive in IIgs cursor-coord deltas already
            // (the inject tool / compositor pre-scaled them), so we
            // bypass the scale to avoid the per-step int-truncation
            // accuracy loss that turned a 320-px traversal into ~306 px.
            // The .which sentinel — set by the agent before SDL_PushEvent
            // — is how we tell them apart. See agent/Protocol.hpp.
            const bool from_agent =
                (event.motion.which == agent::protocol::AGENT_MOUSEID);
            const float scale = from_agent
                ? 1.0f
                : (handler == 1 ? MOUSE_MOTION_SCALE_SLOW
                                : MOUSE_MOTION_SCALE_FAST);
            int xrel = (int)(event.motion.xrel * scale);
            int yrel = (int)(event.motion.yrel * scale);

            // Preserve "minimum motion of 1 in the requested direction"
            // for sub-pixel scaled deltas, but only when nothing is
            // already pending in that axis — otherwise we'd corrupt
            // the accumulator's running total with rounding nudges.
            if (xrel == 0 && pending_xrel == 0 && event.motion.xrel > 0) xrel =  1;
            if (xrel == 0 && pending_xrel == 0 && event.motion.xrel < 0) xrel = -1;
            if (yrel == 0 && pending_yrel == 0 && event.motion.yrel > 0) yrel =  1;
            if (yrel == 0 && pending_yrel == 0 && event.motion.yrel < 0) yrel = -1;

            pending_xrel += xrel;
            pending_yrel += yrel;
            // Cap the accumulator to a sane range so a runaway burst
            // can't grow without bound. ±4095 ≈ 65 polls of full-tilt
            // motion in one direction; way past anything realistic.
            if (pending_xrel >  4095) pending_xrel =  4095;
            if (pending_xrel < -4095) pending_xrel = -4095;
            if (pending_yrel >  4095) pending_yrel =  4095;
            if (pending_yrel < -4095) pending_yrel = -4095;

            has_data = true;
            status = true;
        }
        //if (status) print_registers();
        return status;
    }
};
