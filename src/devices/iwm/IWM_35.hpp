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
        
        void set_enable(bool enable) override {
            enabled = enable;
            led_status = enable;
            // TODO: lock media in drive
            // 3.5 enable does not determine motor_on
        }
        
        // Implementations of the StorageDevice interface
        bool mount(uint64_t key, media_descriptor *media) override {
            // TODO: implement 3.5" mount
            return false;
        }
        
        bool unmount(uint64_t key) override {
            // TODO: implement 3.5" unmount
            return false;
        }
        
        bool writeback(uint64_t key) override {
            // TODO: implement 3.5" writeback
            return false;
        }
        
        drive_status_t status(uint64_t key) override {
            // TODO: implement 3.5" status
            return {false, nullptr, false, 0, false};
        }
};

