#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIe : public VideoScannerII
{
protected:

public:
    VideoScannerIIe(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

