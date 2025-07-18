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

struct cpu_state;
struct debug_window_t; // don't bring in debugwindow.hpp, it would create a depedence on SDL.
struct video_system_t; // same.
class Mounts;
class EventTimer;
class VideoScannerII;

/* typedef void (*reset_handler_t)(void *context);

struct reset_handler_rec {
    reset_handler_t handler;
    void *context;
}; */

struct computer_t {

    using ResetHandler = std::function<bool ()>;
    using ShutdownHandler = std::function<bool ()>;

    cpu_state *cpu = nullptr;
    MMU_II *mmu = nullptr;
    //VideoScannerII *video_scanner = nullptr;
    platform_info *platform = nullptr;
    MessageBus *mbus = nullptr;

    EventDispatcher *sys_event = nullptr;
    EventDispatcher *dispatch = nullptr;

    video_system_t *video_system = nullptr;
    debug_window_t *debug_window = nullptr;

    EventTimer *event_timer = nullptr;

    EventQueue *event_queue = nullptr;

    DeviceFrameDispatcher *device_frame_dispatcher = nullptr;

    Mounts *mounts = nullptr;

    std::vector<ResetHandler> reset_handlers;
    std::vector<ShutdownHandler> shutdown_handlers;
    
    void *module_store[MODULE_NUM_MODULES];
    SlotData *slot_store[NUM_SLOTS];

    computer_t();
    ~computer_t();
    void set_mmu(MMU_II *mmu) { this->mmu = mmu; }
    void set_platform(platform_info *platform) { this->platform = platform; }
    void reset(bool cold_start);

    void register_reset_handler(ResetHandler handler);
    void register_shutdown_handler(ShutdownHandler handler);

    void *get_module_state( module_id_t module_id);
    void set_module_state( module_id_t module_id, void *state);

    SlotData *get_slot_state(SlotType_t slot);
    SlotData *get_slot_state_by_id(device_id id);
    void set_slot_state( SlotType_t slot, SlotData *state);

    void set_slot_irq( uint8_t slot, bool irq);

    void send_clock_mode_message();

};