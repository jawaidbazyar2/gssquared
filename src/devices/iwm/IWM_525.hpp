#pragma once

#include "IWM_Drive.hpp"
#include "util/SoundEffect.hpp"
#include "NClock.hpp"
#include "devices/diskii/Floppy525.hpp"
#include "util/mount.hpp"

class IWM_Drive_525 : public IWM_Drive {
    protected:
        Floppy525 dr;

    public:
        IWM_Drive_525(SoundEffect *sound_effect, NClockII *clock) : IWM_Drive(sound_effect, clock), dr(sound_effect, clock) {
            enabled = false;
        }

        int get_track() override { return dr.get_track(); }
        
        void set_enable(bool enable) override {
            enabled = enable;
            if (motor_on && !enable) {
                // could use a general purpose C++ or SDL timer instead of EventTimer.
                // for now:
                motor_on = enable;
                led_status = enable;
            } else {
                motor_on = enable;
                led_status = enable;
            }

            // TODO: we need a mechanism to schedule motor off if needed.
            dr.set_enable(enable);
        }

        void read_cmd(uint16_t address) override {
            dr.read_cmd(address);
        }

        void write_cmd(uint16_t address, uint8_t data) override {
            dr.write_cmd(address, data);
        }

        uint8_t read_data_register() override {
            return dr.read_nybble();
        }

        void write_data_register(uint8_t data) override {
            dr.write_nybble(data);
        }

        // Implementations of the StorageDevice interface
        bool mount(storage_key_t key, media_descriptor *media) override {
            return dr.mount(key, media);
        }
        
        bool unmount(storage_key_t key) override {
            return dr.unmount(key);
        }
        
        bool writeback(storage_key_t key) override {
            return dr.writeback();
        }
        
        drive_status_t status(storage_key_t key) override {
            return dr.status();
        }
};