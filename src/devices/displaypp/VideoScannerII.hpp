
#pragma once

#include <cstdint>
#include "mmus/mmu_ii.hpp"
#include "frame/Frames.hpp"
#include "gs2.hpp"
#include "ScanBuffer.hpp"
#include "device_irq_id.hpp"

class MMU_II;
struct display_state_t;
struct computer_t;

constexpr uint16_t SCANNER_LUT_SIZE = 65*262;

#define F_SHR    0b1000'0000
#define F_80STORE 0b100'0000
#define F_GRAF   0b010'0000
#define F_HIRES  0b001'0000
#define F_PAGE2  0b000'1000
#define F_80COL  0b000'0100
#define F_DBLRES 0b000'0010
#define F_MIXED  0b000'0001

typedef enum {
    VM_TEXT40 = 0,
    VM_LORES,
    VM_HIRES,
    VM_HIRES_NOSHIFT,
    VM_TEXT80,
    VM_DLORES,
    VM_DHIRES,
    VM_SHR,
    VM_SHR_MODE,
    VM_SHR_PALETTE,
    VM_BORDER_COLOR,
    VM_VSYNC,
    VM_HSYNC,
    VM_LAST_HBL
} video_mode_t;

#define VS_FL_ALTCHARSET 0x01
#define VS_FL_MIXED      0x02
#define VS_FL_80COL      0x04
#define VS_FL_COLORBURST 0x08

#define SA_FLAG_HBL 0x01
#define SA_FLAG_VBL 0x02
#define SA_FLAG_BORDER 0x04
#define SA_FLAG_SCB 0x08
#define SA_FLAG_PALETTE 0x10
#define SA_FLAG_SHR 0x20
#define SA_FLAG_VSYNC 0x40
#define SA_FLAG_HSYNC 0x80
#define SA_FLAG_BLANK (SA_FLAG_HBL | SA_FLAG_VBL)

struct scan_address_t {
    uint16_t addr;
    uint16_t flags;
};

typedef struct mode_table_t {
    scan_address_t *vaddr = nullptr;
    uint8_t mode;
} mode_table_t;

typedef scan_address_t scanner_lut_t[SCANNER_LUT_SIZE];

class VideoScannerII
{
protected:
    // LUTs for video addresses
    /* alignas(64) scanner_lut_t lores_p1;
    alignas(64) scanner_lut_t lores_p2;
    alignas(64) scanner_lut_t hires_p1;
    alignas(64) scanner_lut_t hires_p2;
    alignas(64) scanner_lut_t mixed_p1;
    alignas(64) scanner_lut_t mixed_p2;
    alignas(64) scanner_lut_t shr_p1; */

    scan_address_t *lores_p1 = nullptr;
    scan_address_t *lores_p2 = nullptr;
    scan_address_t *hires_p1 = nullptr;
    scan_address_t *hires_p2 = nullptr;
    scan_address_t *mixed_p1 = nullptr;
    scan_address_t *mixed_p2 = nullptr;
    scan_address_t *shr_p1 = nullptr;

    scan_address_t *video_addresses;
    
    uint32_t cycles_per_frame = 17030;

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

    ScanBuffer *frame_scan = nullptr;
    uint32_t scan_index = 0;

    // floating bus video data
    uint8_t   video_byte;

    bool      graf = false;
    bool      hires = false;
    bool      mixed = false;
    bool      page2 = false;

    // IIe
    bool      sw80col = false;
    bool      altchrset = false;
    bool      dblres = false;
    bool      f_80store = false;

    // IIGS
    uint16_t   text_bg = 0x00;
    uint16_t   text_fg = 0x0F;
    uint8_t    text_color = 0xF0;
    uint16_t   border_color = 0x00;
    bool      shr = false;

    uint8_t   mode_flags = 0;

    video_mode_t video_mode;
    uint8_t vmode = 0;
    mode_table_t mode_table[256];

    MMU_II * mmu = nullptr;

    device_irq_handler_s irq_handler = {nullptr, nullptr};
    mode_table_t calc_video_mode_x(uint8_t vmode);
    virtual void init_mode_table();

    uint8_t current_scb = 0;
    uint16_t h_counter = 0;

public:
uint32_t  hcount;       // use separate hcount and vcount in order
//uint32_t  vcount;       // to simplify IIgs scanline interrupts
    
    VideoScannerII(MMU_II *mmu);
    virtual ~VideoScannerII() = default;

    // Call this after construction to properly initialize video addresses
    virtual void initialize() { init_video_addresses(); }
    virtual void allocate();

    virtual void reset() { frame_scan->clear(); hcount = 0; scan_index = 7 /* (65*243) */; };

    virtual void video_cycle();
    virtual void init_video_addresses();

    inline bool is_hbl()     { return (scan_index % 65) < 25;   }
    inline bool is_vbl()     { return scan_index >= (192*65); }
    inline uint16_t get_vcount() { return scan_index / 65; }
    inline uint16_t get_hcount() { return scan_index % 65; }

    inline uint16_t get_hcounter() {
        uint16_t hcounter;
        uint16_t horz = (scan_index % 65);
        if (horz == 0) hcounter = 0;
        else hcounter = (0x40 + horz - 1);
        return hcounter;
    }
    inline uint16_t get_vcounter() { 
        uint16_t vcounter;
        uint16_t vert = scan_index / 65;
        if (vert < 192) vcounter = 0x100 + vert;
        else if (vert < 256) vcounter = 0x1C0 + (vert - 192);
        else vcounter = 0xFA + (vert - 256);
        return vcounter;
    }

    virtual void set_video_mode();
    inline void set_page_1() { page2 = false; set_video_mode(); }
    inline void set_page_2() { page2 = true;  set_video_mode(); }
    inline void set_full()   { mixed = false; set_video_mode(); }
    inline void set_mixed()  { mixed = true;  set_video_mode(); }
    inline void set_lores()  { hires = false; set_video_mode(); }
    inline void set_hires()  { hires = true;  set_video_mode(); }
    inline void set_text()   { graf  = false; set_video_mode(); }
    inline void set_graf()   { graf  = true;  set_video_mode(); }
    inline void set_80store(bool fl) { f_80store = fl; set_video_mode(); }
    inline void set_shr() { shr = true; set_video_mode(); }

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
    inline void reset_shr()       { shr       = false; set_video_mode(); }

    inline void set_text_bg(uint16_t bg) { text_bg = bg; text_color = text_fg << 4 | text_bg; }
    inline void set_text_fg(uint16_t fg) { text_fg = fg; text_color = text_fg << 4 | text_bg; }
    inline void set_border_color(uint16_t color) { border_color = color; }

    inline virtual void set_irq_handler(device_irq_handler_s irq_handler) { this->irq_handler = irq_handler; }

    ScanBuffer *get_frame_scan();
};

void init_mb_video_scanner(computer_t *computer, SlotType_t slot);
