#pragma once

#include "IWM_Drive.hpp"
#include "util/SoundEffect.hpp"
#include "NClock.hpp"

class IWM_Drive_35 : public IWM_Drive {
    protected:

    public:
        IWM_Drive_35(SoundEffect *sound_effect, NClockII *clock) : IWM_Drive(sound_effect, clock) {
            enabled = false;
        }

        int get_track() override { return 0; } // TODO: implement 3.5" get_track

        void set_enable(bool enable) override {
            enabled = enable;
            led_status = enable;
            // TODO: lock media in drive
            // 3.5 enable does not determine motor_on
        }
        
        // Implementations of the StorageDevice interface
        bool mount(storage_key_t key, media_descriptor *media) override {
            // TODO: implement 3.5" mount
            return false;
        }
        
        bool unmount(storage_key_t key) override {
            // TODO: implement 3.5" unmount
            return false;
        }
        
        bool writeback(storage_key_t key) override {
            // TODO: implement 3.5" writeback
            return false;
        }
        
        drive_status_t status(storage_key_t key) override {
            // TODO: implement 3.5" status
            return {false, nullptr, false, 0, false};
        }
};

