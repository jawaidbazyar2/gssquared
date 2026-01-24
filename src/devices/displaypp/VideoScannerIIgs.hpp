#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIgs : public VideoScannerII
{
protected:
    uint8_t palette_index = 0;
public:
    VideoScannerIIgs(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

//void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

