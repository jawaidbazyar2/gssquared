#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIgs : public VideoScannerII
{
protected:
    static constexpr uint8_t delay_lut_iigs[static_cast<uint8_t>(vs_mode_switch_t::COUNT)] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1
    };

    inline virtual bool supports_dblres() const override { return true; }
    inline uint8_t mode_change_delay(vs_mode_switch_t sw) const override {
        return delay_lut_iigs[static_cast<uint8_t>(sw)];
    }
    uint8_t palette_index = 0;
public:
    VideoScannerIIgs(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
    virtual void dump_cycles() ;
};

//void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

