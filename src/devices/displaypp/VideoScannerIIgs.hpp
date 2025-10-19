#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIgs : public VideoScannerII
{
protected:

public:
    VideoScannerIIgs(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

