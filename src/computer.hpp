#pragma once

#include <vector>

#include "mmus/mmu_ii.hpp"
#include "Module_ID.hpp"
#include "SlotData.hpp"
#include "util/EventDispatcher.hpp"
#include "util/EventQueue.hpp"
#include "util/DeviceFrameDispatcher.hpp"
#include "platforms.hpp"
#include "mbus/MessageBus.hpp"
#include "util/DebugFormatter.hpp"
#include "util/Metrics.hpp"
#include "devices/displaypp/VideoScanner.hpp"
#include "util/InterruptController.hpp"
#include "util/AudioSystem.hpp"
#include "util/SoundEffect.hpp"

class SlotManager_t; // forward declare.

struct cpu_state;
struct debug_window_t; // don't bring in debugwindow.hpp, it would create a depedence on SDL.
struct video_system_t; // same.
class Mounts;
class EventTimer;
class VideoScannerII;


enum execution_modes_t {
    EXEC_NORMAL = 0,
    EXEC_STEP_INTO,
    //EXEC_STEP_OVER // no longer used?
};


struct computer_t {

    using ResetHandler = std::function<bool ()>;
    using ShutdownHandler = std::function<bool ()>;
    using DebugDisplayHandler = std::function<DebugFormatter *()>;
    
    struct DebugDisplayHandlerInfo {
        std::string name;
        uint64_t id;
        DebugDisplayHandler handler;
    };

    computer_t(NClockII *clock);
    ~computer_t();

    cpu_state *cpu = nullptr;
    MMU_II *mmu = nullptr;
    //VideoScannerII *video_scanner = nullptr;
    platform_info *platform = nullptr;
    MessageBus *mbus = nullptr;

    SlotManager_t *slot_manager = nullptr;
    InterruptController *irq_control = nullptr;

    // handle speed shift requests (between frames)
    bool speed_shift = false;
    clock_mode_t speed_new = CLOCK_1_024MHZ;

    EventDispatcher *sys_event = nullptr;
    EventDispatcher *dispatch = nullptr;

    video_system_t *video_system = nullptr;
    debug_window_t *debug_window = nullptr;

    AudioSystem *audio_system = nullptr;
    SoundEffect *sound_effect = nullptr;
    
    EventTimer *event_timer = nullptr;
    EventTimer *vid_event_timer = nullptr;

    EventQueue *event_queue = nullptr;

    DeviceFrameDispatcher *device_frame_dispatcher = nullptr;

    Mounts *mounts = nullptr;

    std::vector<ResetHandler> reset_handlers;
    std::vector<ShutdownHandler> shutdown_handlers;
    std::vector<DebugDisplayHandlerInfo> debug_display_handlers;
    
    void *module_store[MODULE_NUM_MODULES];

    // Status, Statistics, etc.
    Metrics event_times, audio_times, app_event_times, display_times, device_times;
    uint64_t frame_count = 0, status_count = 0;
    uint64_t last_5sec_cycles = 0;
    uint64_t last_frame_end_time = 0, last_5sec_update = 0;
    uint64_t frame_start_cycle = 0;

    video_scanner_t video_scanner = Scanner_AppleII;
    NClockII *clock = nullptr;

    // controls for single-step
    execution_modes_t execution_mode = EXEC_NORMAL;
    uint64_t instructions_left = 0;

    void set_clock(NClockII *clock); 
    
    void set_mmu(MMU_II *mmu) { this->mmu = mmu; }
    void set_cpu(cpu_state *cpu) { this->cpu = cpu; }
    void set_platform(platform_info *platform) { this->platform = platform; }
    void set_video_scanner(video_scanner_t video_scanner) { this->video_scanner = video_scanner; }
    video_scanner_t get_video_scanner() { return video_scanner; }
    void reset(bool cold_start);
    void set_frame_start_cycle();
    uint64_t get_frame_start_cycle() { return frame_start_cycle; }

    void register_reset_handler(ResetHandler handler);
    void register_shutdown_handler(ShutdownHandler handler);
    void register_debug_display_handler(std::string name, uint64_t id, DebugDisplayHandler handler);
    DebugFormatter *call_debug_display_handler(std::string name);

    void *get_module_state( module_id_t module_id);
    void set_module_state( module_id_t module_id, void *state);

    void send_clock_mode_message(clock_mode_t clock_mode);
    void frame_status_update();

};