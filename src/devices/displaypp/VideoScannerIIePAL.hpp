#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIePAL : public VideoScannerII
{
    private:
        const uint32_t SCANNER_CYCLES = (65 * 312);
        
protected:

public:
    VideoScannerIIePAL(MMU_II *mmu);

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

void init_mb_video_scanner_iiepal(computer_t *computer, SlotType_t slot);

