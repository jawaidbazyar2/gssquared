#pragma once

#include "computer.hpp"
#include "SlotData.hpp"

/* This file is little-endian dependent */

static const uint32_t IWM_SWITCH_COUNT = 8;
static const uint32_t IWM_ADDRESS_MAX = IWM_SWITCH_COUNT * 2;

enum iwm_switch_t {
    IWM_CA0_OFF = 0,
    IWM_CA0_ON = 1,
    IWM_CA1_OFF = 2,
    IWM_CA1_ON = 3,
    IWM_CA2_OFF = 4,
    IWM_CA2_ON = 5,
    IWM_LSTRB_OFF = 6,
    IWM_LSTRB_ON = 7,
    IWM_ENABLE_OFF = 8,
    IWM_ENABLE_ON = 9,
    IWM_SELECT_OFF = 10,
    IWM_SELECT_ON = 11,
    IWM_Q6_OFF = 12,
    IWM_Q6_ON = 13,
    IWM_Q7_OFF = 14,
    IWM_Q7_ON = 15,
};

class IWM_Drive {
    protected:
    bool enabled = false;
    bool motor_on = false;
    bool sense_input = false;
    bool led_status = false;

    public:
    IWM_Drive() {
        enabled = false;
        motor_on = false;
        sense_input = false;
        led_status = false;
    }
    virtual ~IWM_Drive() {};
    virtual void set_enable(bool enable) {};
    bool get_enabled() { return enabled; }
    bool get_motor_on() { return motor_on; }
    bool get_sense_input() { return sense_input; }
    bool get_led_status() { return led_status; }
    virtual void write_data_register(uint8_t data) {};
    virtual uint8_t read_data_register() { return 0; };
};

class IWM_Drive_525 : public IWM_Drive {
    public:
        IWM_Drive_525() {
            enabled = false;
        }
        void set_enable(bool enable) override {
            enabled = enable;
            if (motor_on && !enable) {
                // TODO: schedule a motor off for 1 second from now.
                // could use a general purpose C++ or SDL timer instead of EventTimer.
                // for now:
                motor_on = enable;
                led_status = enable;
            } else {
                motor_on = enable;
                led_status = enable;
            }
            
        }
};

class IWM_Drive_35 : public IWM_Drive {
    public:
        IWM_Drive_35() {
            enabled = false;
        }
        void set_enable(bool enable) override {
            enabled = enable;
            led_status = enable;
            // TODO: lock media in drive
            // 3.5 enable does not determine motor_on
        }
};


class IWM {
    public:
        IWM() {
            for (uint32_t i = 0; i < IWM_SWITCH_COUNT; i++) {
                switches[i] = 0;
            }
            drives[0] = new IWM_Drive_525();
            drives[1] = new IWM_Drive_525();
            drives[2] = new IWM_Drive_35();
            drives[3] = new IWM_Drive_35();
            reset();
/*             disk_register = 0;
            for (uint32_t i = 0; i < 4; i++) {
                drives[i]->set_enable(false);
            }
            drive_selected = 0;
            reg_mode = 0;
            reg_handshake = 0; */
        };

        void reset() {
            disk_register = 0;
            for (uint32_t i = 0; i < 4; i++) {
                drives[i]->set_enable(false);
            }
            drive_selected = 0;
            reg_mode = 0;
            reg_handshake = 0;
        }

        // utility functions
        inline bool address_odd(uint32_t address) { return address & 0x01; }
        inline bool address_even(uint32_t address) { return (address & 0x01) == 0; }
        //uint32_t register_index() { }

        ~IWM() {};
        void set_switch(uint32_t switch_index, bool onoff) { 
            assert(switch_index < IWM_SWITCH_COUNT && "IWM: switch index out of bounds");
            switches[switch_index] = onoff; 
        }
        bool get_switch(uint32_t switch_index) { 
            assert(switch_index < IWM_SWITCH_COUNT && "IWM: switch index out of bounds");
            return switches[switch_index]; 
        }

        uint8_t read_disk_register() { return disk_register; }
        void write_disk_register(uint8_t data) { disk_register = data; }

        uint8_t read_status_register() {
            // TODO: change any_drive_on and sense_input to query selected disk statuses later
            return (reg_mode & 0b000'11111) | any_drive_on << 5 | sense_input << 7;
        }
        inline void handle_switch(uint32_t address) {
            switch (address) {
                case IWM_ENABLE_ON:
                    any_enabled = true;
                    drives[drive_selected]->set_enable(true);
                    break;
                case IWM_ENABLE_OFF:
                    any_enabled = false;
                    drives[drive_selected]->set_enable(false);
                    break;
                case IWM_SELECT_ON:
                    drives[drive_selected]->set_enable(false); // de-select     
                    drive_selected = 1;
                    drives[drive_selected]->set_enable(any_enabled);
                    break;
                case IWM_SELECT_OFF:
                    drives[drive_selected]->set_enable(false); // de-select 
                    drive_selected = 0;
                    drives[drive_selected]->set_enable(any_enabled);
                    break;
                default:
                    break;
            }
        }

        uint8_t read(uint32_t address) {
            assert(address < IWM_ADDRESS_MAX && "IWM: read address out of bounds");
            access(address);
            
            handle_switch(address);

            /* Read Status Register 
            To access it, turn Q7 off and Q6 on, and read from any even-numbered address in the
            $C0E0...$C0EF range.
            */
            if (address_even(address) && !iwm_q7 && iwm_q6) {
                return read_status_register();
            }
            /* The handshake register is a read-only register used when writing to the
            disk in asynchronous mode (when bit 1 of the mode register is on). It
            indicates whether the IWM is ready to receive the next data byte. To
            read the handshake register, turn switches Q6 off and Q7 on, and read
            from any even-numbered address  */
            if (address_even(address) && !iwm_q6 && iwm_q7) {
                return reg_handshake;                
            }
            /* The data register is the register that you read to get the actual data
            from the disk and write to store data on the disk. To read it, turn Q6
            and Q7 off and read from any even-numbered address in the $C0E0...$C0EF
            range. */
            if (address_even(address) && !iwm_q6 && !iwm_q7) {
                return drives[drive_selected]->read_data_register();
            }
            return 0;
        }

        void write(uint32_t address, uint8_t data) { 
            assert(address < IWM_ADDRESS_MAX && "IWM: write address out of bounds");
            access(address);

            handle_switch(address);

            /*
            Note that the drive may remain active for a second or two after the ENABLE
            access, and that the write to the mode register will fail unless the drive
            is fully deactivated.
            This means that the mode register must be repeatedly
            written until the status register (see below) indicates that the desired
            changes have taken effect.
            */
            if (address_odd(address) && !any_enabled && !any_drive_on && iwm_q6 && iwm_q7) { // write to mode register.
                reg_mode = data;
            }
            /* To write it, turn Q6 and Q7 on and write to any odd-numbered
            address in the $C0E0...$C0EF range. */
            if (address_odd(address) && iwm_q6 && iwm_q7) {
                drives[drive_selected]->write_data_register(data);
            }
        }

        void debug_output(DebugFormatter *df) {
            df->addLine("CA0: %d, CA1: %d, CA2: %d, LSTRB: %d, ENABLE: %d, SELECT: %d, Q6: %d, Q7: %d",
                iwm_ca0, iwm_ca1, iwm_ca2, iwm_lstrb, iwm_enable, iwm_select, iwm_q6, iwm_q7);
            df->addLine("Disk Register: %02X", disk_register);
            df->addLine("Mode: %02X  ClkSpd: %d  BitCell: %d  MotorOff: %d  HSProtocol: %d", reg_mode, mr_clockspeed, mr_bitcelltime, mr_motorofftimer, mr_hsprotocol);
            df->addLine("Handshake: %02X  Status: %02X", reg_handshake, read_status_register());
            df->addLine(  "         5.25/1     5.25/2     3.5/1     3.5/2");
            df->addLine("Drive Selected: %d", drive_selected);
            df->addLine("  Motor:   %d          %d          %d          %d      ", 
                drives[0]->get_motor_on(), 
                drives[1]->get_motor_on(), 
                drives[2]->get_motor_on(), 
                drives[3]->get_motor_on());
            df->addLine("  Sense:   %d          %d          %d          %d      ", 
                drives[0]->get_sense_input(), 
                drives[1]->get_sense_input(), 
                drives[2]->get_sense_input(), 
                drives[3]->get_sense_input());
            df->addLine("  LED:     %d          %d          %d          %d      ", 
                drives[0]->get_led_status(), 
                drives[1]->get_led_status(), 
                drives[2]->get_led_status(), 
                drives[3]->get_led_status());
        }

    private:
        /* You can access the switch states either by array index, or by individual switch name */
        union {
            uint32_t switches[IWM_SWITCH_COUNT];
            struct {
                uint32_t iwm_ca0;
                uint32_t iwm_ca1;
                uint32_t iwm_ca2;
                uint32_t iwm_lstrb;
                uint32_t iwm_enable;
                uint32_t iwm_select;
                uint32_t iwm_q6;
                uint32_t iwm_q7;
            };        
        };

        IWM_Drive *drives[4];

        uint32_t drive_selected = 0; // 0 = 5.25 1, 1 = 5.25 2, 2 = 3.5 1, 3 = 3.5 2
        uint8_t any_enabled = 0;
        uint8_t any_drive_on = 0;
        uint8_t sense_input = 0;
        union {
            struct {
                uint8_t dr_reserved: 6;
                uint8_t dr_enable35 : 1;
                uint8_t dr_sel : 1;
            };
            uint8_t disk_register;
        };

        union {
            struct {
                uint8_t mr_latch : 1;
                uint8_t mr_hsprotocol : 1;
                uint8_t mr_motorofftimer : 1;
                uint8_t mr_bitcelltime : 1;
                uint8_t mr_clockspeed : 1;
                uint8_t mr_reserved : 3;
            };
            uint8_t reg_mode; // write only
        };
        union {
            struct {
                uint8_t hr_reserved : 6;
                uint8_t hr_underrun : 1;
                uint8_t hr_register_ready : 1;
            };
            uint8_t reg_handshake;
        };

        /*
        * Handles state changes on "access" to registers C0E0-C0EF, which apply to either reads or writes.
        */
        void access(uint32_t address) { // address must be between 0 and 0x0F
            assert(address <= 0x0F && "IWM: access address out of bounds");
            uint32_t switch_index = (address >> 1);
            bool onoff = (address & 0x01) == 1;
            set_switch(switch_index, onoff); 
        }

};


