#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIe : public VideoScannerII
{
protected:
    static constexpr uint8_t delay_lut_iie[static_cast<uint8_t>(vs_mode_switch_t::COUNT)] = {
        // TEXT, MIXED, PAGE2, HIRES, STORE80, COL80, ALTCHAR, DBLRES, SHR
        1, 1, 0, 0, 0, 0, 0, 1, 1
    };

    inline virtual bool supports_dblres() const override { return true; }
    inline uint8_t mode_change_delay(vs_mode_switch_t sw) const override {
        return delay_lut_iie[static_cast<uint8_t>(sw)];
    }

public:
    VideoScannerIIe(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

