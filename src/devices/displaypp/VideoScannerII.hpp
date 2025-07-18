
#pragma once

#include <cstdint>
#include "mmus/mmu.hpp"
#include "computer.hpp"
#include "frame/Frames.hpp"

typedef enum {
    VM_TEXT40 = 0,
    VM_ALT_TEXT40,
    VM_LORES,
    VM_HIRES,
    VM_HIRES_NOSHIFT,
    VM_LORES_MIXED,
    VM_LORES_ALT_MIXED,
    VM_HIRES_MIXED,
    VM_HIRES_ALT_MIXED,
    VM_HIRES_NOSHIFT_MIXED,
    VM_HIRES_NOSHIFT_ALT_MIXED,
    VM_TEXT80,
    VM_ALT_TEXT80,
    VM_LORES_MIXED80,
    VM_LORES_ALT_MIXED80,
    VM_HIRES_MIXED80,
    VM_HIRES_ALT_MIXED80,
    VM_DLORES,
    VM_DLORES_MIXED80,
    VM_DLORES_ALT_MIXED80,
    VM_DHIRES,
    VM_DHIRES_MIXED80,
    VM_DHIRES_ALT_MIXED80,
    VM_SHR320,
    VM_SHR640,
    VM_PALETTE_DATA,
    VM_BORDER_COLOR,
    VM_LAST_HBL
} video_mode_t;

/* typedef enum {
    VM_TEXT40 = 0,
    VM_TEXT80,
    VM_LORES,
    VM_LORES80,
    VM_HIRES,
    VM_HIRES80,
    VM_SHR320,
    VM_SHR640,
    VM_PALETTE_DATA,
    VM_BORDER_COLOR,
    VM_LAST_HBL
} video_mode_t; */

class VideoScannerII
{
protected:
    // LUTs for video addresses
    uint16_t lores_p1_addresses[65*262];
    uint16_t lores_p2_addresses[65*262];
    uint16_t hires_p1_addresses[65*262];
    uint16_t hires_p2_addresses[65*262];
    uint16_t mixed_p1_addresses[65*262];
    uint16_t mixed_p2_addresses[65*262];

    uint16_t (*video_addresses)[65*262];
    
    // video mode/memory data
    // 5*40*200: 40*200 video states for SHR, 1 mode byte + 4 data bytes for each state
    // 2*13*200: 13*200 horz border states, 1 mode byte + 1 data byte for each state
    // 2*53*40   53*40  vert border states, 1 mode byte + 1 data byte for each state
    // 33*200    200 SHR palettes, 1 mode byte + 32 data bytes per palette
    // 2*192     192 lines in legacy modes, 1 mode byte + 1 last HBL data byte for each line
    static const int video_data_max = 5*40*200 + 2*13*20 + 2*53*40 + 33*200 + 2*192;
    uint8_t   video_data[video_data_max];
    int       video_data_size;
    
    uint8_t *ram;
    FrameScan560 *frame_scan = nullptr;

    // floating bus video data
    uint8_t   video_byte;

    uint32_t  hcount;       // use separate hcount and vcount in order
    uint32_t  vcount;       // to simplify IIgs scanline interrupts

    bool      graf;
    bool      hires;
    bool      mixed;
    bool      page2;

    video_mode_t video_mode;

    MMU_II * mmu;

    virtual void set_video_mode();

public:
    VideoScannerII(uint8_t *ram);

    virtual void video_cycle();
    virtual void init_video_addresses();

    inline int       get_video_data_size() { return video_data_size; }
    inline void      end_video_cycle()     { video_data_size = 0; }
    inline uint8_t   get_video_byte()      { return video_byte; }
    inline uint8_t * get_video_data()      { return video_data; }

    inline bool is_hbl()     { return hcount < 25;   }
    inline bool is_vbl()     { return vcount >= 192; }

    inline void set_page_1() { page2 = false; set_video_mode(); }
    inline void set_page_2() { page2 = true;  set_video_mode(); }
    inline void set_full()   { mixed = false; set_video_mode(); }
    inline void set_mixed()  { mixed = true;  set_video_mode(); }
    inline void set_lores()  { hires = false; set_video_mode(); }
    inline void set_hires()  { hires = true;  set_video_mode(); }
    inline void set_text()   { graf  = false; set_video_mode(); }
    inline void set_graf()   { graf  = true;  set_video_mode(); }

    inline bool is_page_1() { return !page2; }
    inline bool is_page_2() { return  page2; }
    inline bool is_full()   { return !mixed; }
    inline bool is_mixed()  { return  mixed; }
    inline bool is_lores()  { return !hires; }
    inline bool is_hires()  { return  hires; }
    inline bool is_text()   { return !graf;  }
    inline bool is_graf()   { return  graf;  }

    FrameScan560 *get_frame_scan();
};

uint8_t vs_bus_read_C050(void *context, uint16_t address);
uint8_t vs_bus_read_C051(void *context, uint16_t address);
uint8_t vs_bus_read_C052(void *context, uint16_t address);
uint8_t vs_bus_read_C053(void *context, uint16_t address);
uint8_t vs_bus_read_C054(void *context, uint16_t address);
uint8_t vs_bus_read_C055(void *context, uint16_t address);
uint8_t vs_bus_read_C056(void *context, uint16_t address);
uint8_t vs_bus_read_C057(void *context, uint16_t address);

void vs_bus_write_C050(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C051(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C052(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C053(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C054(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C055(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C056(void *context, uint16_t address, uint8_t value);
void vs_bus_write_C057(void *context, uint16_t address, uint8_t value);

void init_mb_video_scanner(computer_t *computer, SlotType_t slot);
