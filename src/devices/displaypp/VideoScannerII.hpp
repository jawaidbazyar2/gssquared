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
    VM_LORES_7M,
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
    VM_LAST_HBL,
    VM_BLANK,
} video_mode_t;

constexpr const char *video_mode_names[] = {
    "TEXT40",
    "LORES",
    "LORES_7M",
    "HIRES",
    "HIRES_NOSHIFT",
    "TEXT80",
    "DLORES",
    "DHIRES",
    "SHR",
    "SHR_MODE",
    "SHR_PALETTE",
    "BORDER_COLOR",
    "VSYNC",
    "HSYNC",
    "LAST_HBL",
    "BLANK",
};

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

// Softswitches whose display effect may be delayed by N video cycles.
// Per-platform LUT latencies: 0 = patch prior ScanBuffer entry (same cycle),
// 1 or 2 = take effect at scan_index + N on the next video_cycle drain(s).
enum class vs_mode_switch_t : uint8_t {
    TEXT_GRAF = 0, // C050/C051 (graf)
    MIXED,         // C052/C053
    PAGE2,         // C054/C055
    HIRES,         // C056/C057
    STORE80,       // C000/C001
    COL80,         // C00C/C00D
    ALTCHAR,       // C00E/C00F
    DBLRES,        // C05E/C05F AN3
    SHR,           // C029 NEWVIDEO
    COUNT
};

struct mode_change_t {
    uint16_t apply_at; // scan_index when change takes effect
    vs_mode_switch_t sw;
    uint8_t value;     // 0/1
};

class VideoScannerII
{
protected:
    static constexpr uint8_t MODE_Q_SIZE = 8;
    static constexpr uint8_t delay_lut_ii[static_cast<uint8_t>(vs_mode_switch_t::COUNT)] = {
        1, 1, 0, 0, 1, 1, 1, 1, 1
    };

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
    inline virtual bool supports_dblres() const { return false; }

    uint8_t current_scb = 0;
    uint16_t h_counter = 0;

    // Delayed softswitch → display mode pipeline
    mode_change_t mode_q[MODE_Q_SIZE];
    uint8_t mode_q_head = 0;
    uint8_t mode_q_tail = 0;
    uint8_t mode_q_count = 0;

    void apply_mode_change(vs_mode_switch_t sw, uint8_t value);
    void cancel_pending_mode_change(vs_mode_switch_t sw);
    void queue_mode_change(vs_mode_switch_t sw, uint8_t value);
    void drain_due_mode_changes();
    void clear_mode_change_queue();

    // Per-platform latency; override in IIe / IIgs / PAL.
    inline virtual uint8_t mode_change_delay(vs_mode_switch_t sw) const {
        return delay_lut_ii[static_cast<uint8_t>(sw)];
    }

    // Hot path: empty queue is the common case — avoid the out-of-line call.
    inline void apply_due_mode_changes() {
        if (mode_q_count) {
            drain_due_mode_changes();
        }
    }

public:
//uint32_t  hcount;       // use separate hcount and vcount in order
//uint32_t  vcount;       // to simplify IIgs scanline interrupts
    
    VideoScannerII(MMU_II *mmu);
    virtual ~VideoScannerII();

    // Call this after construction to properly initialize video addresses
    virtual void initialize() { init_video_addresses(); }
    virtual void allocate();

    virtual void reset() {
        frame_scan->clear();
        scan_index = 0;
        clear_mode_change_queue();
    };

    virtual void video_cycle();
    uint32_t get_scan_cycle() { return scan_index; }
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
    inline void set_page_1() { queue_mode_change(vs_mode_switch_t::PAGE2, 0); }
    inline void set_page_2() { queue_mode_change(vs_mode_switch_t::PAGE2, 1); }
    inline void set_full()   { queue_mode_change(vs_mode_switch_t::MIXED, 0); }
    inline void set_mixed()  { queue_mode_change(vs_mode_switch_t::MIXED, 1); }
    inline void set_lores()  { queue_mode_change(vs_mode_switch_t::HIRES, 0); }
    inline void set_hires()  { queue_mode_change(vs_mode_switch_t::HIRES, 1); }
    inline void set_text()   { queue_mode_change(vs_mode_switch_t::TEXT_GRAF, 0); }
    inline void set_graf()   { queue_mode_change(vs_mode_switch_t::TEXT_GRAF, 1); }
    inline void set_80store(bool fl) { queue_mode_change(vs_mode_switch_t::STORE80, fl ? 1 : 0); }
    inline void set_shr() { queue_mode_change(vs_mode_switch_t::SHR, 1); }

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

    inline void set_80col()       { queue_mode_change(vs_mode_switch_t::COL80, 1); }
    inline void set_altchrset()   { queue_mode_change(vs_mode_switch_t::ALTCHAR, 1); }
    inline void set_dblres()      { queue_mode_change(vs_mode_switch_t::DBLRES, 1); }
    inline void set_dblres_f(bool fl) { queue_mode_change(vs_mode_switch_t::DBLRES, fl ? 1 : 0); }
    inline void set_80col_f(bool fl) { queue_mode_change(vs_mode_switch_t::COL80, fl ? 1 : 0); }
    inline void set_altchrset_f(bool fl) { queue_mode_change(vs_mode_switch_t::ALTCHAR, fl ? 1 : 0); }

    inline void reset_80col()     { queue_mode_change(vs_mode_switch_t::COL80, 0); }
    inline void reset_altchrset() { queue_mode_change(vs_mode_switch_t::ALTCHAR, 0); }
    inline void reset_dblres()    { queue_mode_change(vs_mode_switch_t::DBLRES, 0); }
    inline void reset_shr()       { queue_mode_change(vs_mode_switch_t::SHR, 0); }

    inline void set_text_bg(uint16_t bg) { text_bg = bg; text_color = text_fg << 4 | text_bg; }
    inline void set_text_fg(uint16_t fg) { text_fg = fg; text_color = text_fg << 4 | text_bg; }
    inline void set_border_color(uint16_t color) { border_color = color; }

    inline virtual void set_irq_handler(device_irq_handler_s irq_handler) { this->irq_handler = irq_handler; }

    ScanBuffer *get_frame_scan();
};

void init_mb_video_scanner(computer_t *computer, SlotType_t slot);
