
#pragma once

#include "VideoScannerII.hpp"

class VideoScannerIIe : public VideoScannerII
{
protected:
    bool sw80col;
    bool sw80store;
    bool altchrset;
    bool dblres;

    virtual void set_video_mode() override;

public:
    VideoScannerIIe(uint8_t *ram);

    inline bool is_80col()        { return sw80col;   }
    inline bool is_80store()      { return sw80store; }
    inline bool is_altchrset()    { return altchrset; }
    inline bool is_dblres()       { return altchrset; }

    inline void set_80col()       { sw80col   = true;  set_video_mode(); }
    inline void set_80store()     { sw80store = true;  set_video_mode(); }
    inline void set_altchrset()   { altchrset = true;  set_video_mode(); }
    inline void set_dblres()      { dblres    = true;  set_video_mode(); }
    
    inline void reset_80col()     { sw80col   = false; set_video_mode(); }
    inline void reset_80store()   { sw80store = false; set_video_mode(); }
    inline void reset_altchrset() { altchrset = false; set_video_mode(); }
    inline void reset_dblres()    { dblres    = false; set_video_mode(); }

    virtual void video_cycle() override;
    virtual void init_video_addresses() override;
};

void init_mb_video_scanner_iie(computer_t *computer, SlotType_t slot);

void vs_bus_write_C00C(void *context, uint16_t address, uint8_t data);
void vs_bus_write_C00D(void *context, uint16_t address, uint8_t data);
void vs_bus_write_C00E(void *context, uint16_t address, uint8_t data);
void vs_bus_write_C00F(void *context, uint16_t address, uint8_t data);

uint8_t vs_bus_read_C019(void *context, uint16_t address);
uint8_t vs_bus_read_C01A(void *context, uint16_t address);
uint8_t vs_bus_read_C01B(void *context, uint16_t address);

uint8_t vs_bus_read_C01D(void *context, uint16_t address);
uint8_t vs_bus_read_C01E(void *context, uint16_t address);
uint8_t vs_bus_read_C01F(void *context, uint16_t address);
