
#pragma once

#include <cstdint>
#include "mmus/mmu_ii.hpp"
#include "frame/Frames.hpp"
#include "gs2.hpp"
#include "ScanBuffer.hpp"

class MMU_II;
struct display_state_t;
struct computer_t;

#define F_GRAF   0b10'0000
#define F_HIRES  0b01'0000
#define F_PAGE2  0b00'1000
#define F_80COL  0b00'0100
#define F_DBLRES 0b00'0010
#define F_MIXED  0b00'0001

typedef enum {
    VM_TEXT40 = 0,
    VM_LORES,
    VM_HIRES,
    VM_HIRES_NOSHIFT,
    VM_TEXT80,
    VM_DLORES,
    VM_DHIRES,
    VM_SHR320,
    VM_SHR640,
    VM_PALETTE_DATA,
    VM_BORDER_COLOR,
    VM_LAST_HBL
} video_mode_t;

#define VS_FL_ALTCHARSET 0x01
#define VS_FL_MIXED      0x02
#define VS_FL_80COL      0x04
#define VS_FL_COLORBURST 0x08

struct scan_address_t {
    uint16_t address;
    uint16_t hcount;
    uint16_t vcount;
    uint16_t unused;
};

typedef struct {
    uint16_t (*vaddr)[65*262];
    uint8_t mode;
} mode_table_t;

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
   /*  static const int video_data_max = 5*40*200 + 2*13*20 + 2*53*40 + 33*200 + 2*192;
    uint8_t   video_data[video_data_max];
    int       video_data_size; */
    
    uint8_t *ram;
    //FrameScan560 *frame_scan = nullptr;
    ScanBuffer *frame_scan = nullptr;

    // floating bus video data
    uint8_t   video_byte;


    bool      graf = false;
    bool      hires = false;
    bool      mixed = false;
    bool      page2 = false;
    bool      sw80col = false;
    bool      altchrset = false;
    bool      dblres = false;
    uint8_t mode_flags = 0;

    video_mode_t video_mode;
    uint8_t vmode = 0;
    mode_table_t mode_table[64];

    MMU_II * mmu = nullptr;

    mode_table_t calc_video_mode_x(uint8_t vmode);
    virtual void set_video_mode();
    virtual void init_mode_table();

public:
uint32_t  hcount;       // use separate hcount and vcount in order
uint32_t  vcount;       // to simplify IIgs scanline interrupts

    VideoScannerII(MMU_II *mmu);
    virtual ~VideoScannerII() = default;

    virtual void reset() { frame_scan->clear(); hcount = 64; vcount = 261; };

    virtual void video_cycle();
    virtual void init_video_addresses();

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

    inline bool is_80col()        { return sw80col;   }
    inline bool is_altchrset()    { return altchrset; }
    inline bool is_dblres()       { return dblres; }

    inline void set_80col()       { sw80col   = true;  set_video_mode(); }
    inline void set_altchrset()   { altchrset = true;  set_video_mode(); }
    inline void set_dblres()      { dblres    = true;  set_video_mode(); }
    inline void set_dblres_f(bool fl) { dblres    = fl;  set_video_mode(); }
    inline void set_80col_f(bool fl) { sw80col   = fl;  set_video_mode(); }
    inline void set_altchrset_f(bool fl) { altchrset = fl;  set_video_mode(); }

    inline void reset_80col()     { sw80col   = false; set_video_mode(); }
    inline void reset_altchrset() { altchrset = false; set_video_mode(); }
    inline void reset_dblres()    { dblres    = false; set_video_mode(); }

    ScanBuffer *get_frame_scan();
};

void init_mb_video_scanner(computer_t *computer, SlotType_t slot);
