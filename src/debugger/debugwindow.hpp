#pragma once

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "cpu.hpp"
#include "util/TextRenderer.hpp"
#include "ui/Container.hpp"
#include "ui/TextInput.hpp"
#include "debugger/MemoryWatch.hpp"
#include "debugger/disasm.hpp"

struct computer_t;
struct video_system_t;

enum debug_panel_t {
    DEBUG_PANEL_TRACE = 0,
    DEBUG_PANEL_MONITOR,
    DEBUG_PANEL_MEMORY,
    DEBUG_PANEL_DEVICES,
    DEBUG_PANEL_COUNT
};


struct debug_window_t {
    computer_t *computer;
    cpu_state *cpu;
    SDL_Window *window;
    SDL_Renderer *renderer;
    int window_width = 800;
    int window_height = 800;
    int window_margin = 5;
    int control_area_height = 100;
    int lines_in_view_area = 10;
    SDL_WindowID window_id;
    bool window_open = false;
    int view_position = 0;
    
    TextRenderer *text_renderer;
    int font_line_height = 14;
    std::vector<Container_t *> containers;
    Container_t *tab_container;
    MemoryWatch memory_watches;
    MemoryWatch breaks;
    uint32_t stepover_bp = 0;
    Disassembler *disasm = nullptr;
    Disassembler *step_disasm = nullptr;
    std::vector<std::string> debug_displays;
    
    int panel_visible[DEBUG_PANEL_COUNT] = {0};
    SDL_Rect pane_area[DEBUG_PANEL_COUNT];

    TextInput_t* mon_textinput;
    std::vector<std::string> mon_display_buffer;
    std::vector<std::string> mon_history;
    int mon_history_position = 0;

    MMU *mmu = nullptr;

    debug_window_t(computer_t *computer);
    ~debug_window_t();

    void render();
    bool handle_event(SDL_Event &event);
    bool is_open();
    void set_open();
    void set_closed();
    void resize(int width, int height);
    void separator_line(debug_panel_t pane, int y);
    void draw_text(debug_panel_t pane, int x, int y, const char *text);
    void resize_window();
    void toggle_panel(debug_panel_t panel);
    void render_pane_trace();
    void render_pane_monitor();
    void render_pane_memory();
    void render_pane_devices();
    void set_panel_visible(debug_panel_t panel, bool visible);
    bool is_pane_first(debug_panel_t pane);
    int num_lines_in_pane(debug_panel_t pane);
    void event_pane_monitor(SDL_Event &event);
    bool handle_pane_event_monitor(SDL_Event &event);
    bool check_pre_breakpoint(cpu_state *cpu);
    bool check_post_breakpoint(system_trace_entry_t *entry);
    void set_mmu(MMU *mmu) { this->mmu = mmu; }

protected:
    void execute_command(const std::string& command);
};
