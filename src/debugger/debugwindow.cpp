#include <SDL3/SDL.h>
#include <algorithm>
#include <iostream>

#include "debugwindow.hpp"
#include "SDL3/SDL_keycode.h"
#include "cpu.hpp"
#include "debugger/trace.hpp"
#include "util/TextRenderer.hpp"
#include "util/HexDecode.hpp"
#include "computer.hpp"
#include "videosystem.hpp"
#include "ui/Container.hpp"
#include "ui/WrapContainer.hpp"
#include "ui/UIContext.hpp"
#include "ui/Button.hpp"
#include "ui/TextInput.hpp"
#include "debugger/disasm.hpp"

debug_window_t::debug_window_t(computer_t *computer) {
    this->computer = computer;
    this->mmu = computer->mmu;
    this->cpu = computer->cpu;

    panel_visible[DEBUG_PANEL_TRACE] = 1; // all default to off, so enable here.

    // create a new window
    window = SDL_CreateWindow("GSSquared Debugger", window_width, window_height, SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN);
    // create a new renderer
    renderer = SDL_CreateRenderer(window, nullptr);

    text_renderer = new TextRenderer(renderer, "fonts/OxygenMono-Regular.ttf", 15.0f);
    text_renderer->set_color(255, 255, 255, 255);
    font_line_height = text_renderer->font_line_height;
    ui_ctx = { renderer, window, text_renderer, nullptr, nullptr };

    window_id = SDL_GetWindowID(window);
    control_area_height = 8 * font_line_height;
    lines_in_view_area = (window_height - control_area_height) / font_line_height;

    // create a container object to hold our tab control buttons
    Style_t CS;
    CS.padding = 2;
    CS.border_width = 0;
    CS.background_color = 0x00000000;
    CS.border_color = 0x00800000;
    CS.hover_color = 0x00000000;

    tab_container = new Container_t(&ui_ctx, CS);
    tab_container->set_position(25, 15);
    tab_container->size(400, 35);
    containers.push_back(tab_container);
    step_container = new Container_t(&ui_ctx, CS);
    step_container->set_position(600, 15);
    step_container->size(400, 35);
    containers.push_back(step_container);
    debug_display_container = new WrapContainer_t(&ui_ctx, CS);
    debug_display_container->set_position(25, 15);
    debug_display_container->size(600, 70);
    debug_display_container->set_visible(false);
    containers.push_back(debug_display_container);

    Style_t SS;
    //SS.background_color = 0x00A1F0FF; // active (20% brighter)
    SS.background_color = 0x00426340; // 50% darker than 0x0084C6FF
    SS.border_color = 0xFFFFFFFF;
    SS.hover_color = 0x606060FF;
    SS.text_color = 0xFFFFFFFF;
    SS.padding = 4;
    SS.border_width = 1;
    
    Button_t* s1 = new Button_t(&ui_ctx, "Trace", SS);
    s1->size(70, 22);
    //s1->set_text_renderer(text_renderer); // set text renderer for the button
    s1->on_click([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_TRACE);
        return true;
    });
    tab_container->add(s1);
    
    Button_t* s2 = new Button_t(&ui_ctx, "Monitor", SS);
    s2->size(70, 22);
    //s2->set_text_renderer(text_renderer); // set text renderer for the button
    s2->on_click([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_MONITOR);
        return true;
    });
    tab_container->add(s2);

    Button_t* s3 = new Button_t(&ui_ctx, "Watch", SS);
    s3->size(70, 22);
    //s3->set_text_renderer(text_renderer); // set text renderer for the button
    s3->on_click([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_MEMORY);
        return true;
    });
    tab_container->add(s3);
    tab_container->layout();

    /* Button_t *b4 = new Button_t(&ui_ctx, "||", SS);
    b4->size(35, 22);
    step_container->add(b4); */
    Button_t *b7 = new Button_t(&ui_ctx, ">", SS);
    b7->size(35, 22);
    b7->on_click([this](const SDL_Event& event) -> bool {
        step_one();
        return true;
    });
    step_container->add(b7);

    Button_t *b6 = new Button_t(&ui_ctx, ">|", SS);
    b6->size(35, 22);
    b6->on_click([this](const SDL_Event& event) -> bool {
        step_out();
        return true;
    });
    step_container->add(b6);

    Button_t *b8 = new Button_t(&ui_ctx, "^", SS);
    b8->size(35, 22);
    b8->on_click([this](const SDL_Event& event) -> bool {
        step_over();
        return true;
    });
    step_container->add(b8);

    Button_t *b5 = new Button_t(&ui_ctx, ">>", SS);
    b5->size(35, 22);
    b5->on_click([this](const SDL_Event& event) -> bool {
        resume();
        return true;
    });
    step_container->add(b5);


    step_container->layout();

    mon_textinput = new TextInput_t(&ui_ctx, "help", SS);
    mon_textinput->set_text_renderer(text_renderer);
    mon_textinput->set_max_length(80);
    mon_textinput->size(600, 20);
    mon_textinput->set_padding(2);
    mon_textinput->set_enter_handler([this](const SDL_Event& event) -> bool {
        // Call the command interpreter.
        execute_command(mon_textinput->get_text());
        return true;
    });

    Style_t scroll_style;
    scroll_style.background_color = 0x00000000;
    scroll_style.border_color = 0x00FFFFFF;
    scroll_style.border_width = 1;
    scroll_style.padding = 0;
    trace_scroll_ = new ScrollBar_t(&ui_ctx, scroll_style);
    trace_scroll_->on_change([this](int pos) { view_position = pos; });
    mon_scroll_ = new ScrollBar_t(&ui_ctx, scroll_style);
    mon_scroll_->on_change([this](int pos) { mon_view_position = pos; });

    resize_window(); // first time we need to calculate the pane locations and window size.
}

debug_window_t::~debug_window_t() {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    delete text_renderer;
    delete tab_container;
    delete step_container;
    delete debug_display_container;
    delete mon_textinput;
    delete trace_scroll_;
    delete mon_scroll_;
    if (disasm) delete disasm;
    if (step_disasm) delete step_disasm;
}

#include "debugger/BreakpointTable.hpp"
#include "debugger/DebugProtocolServer.hpp"

bool debug_window_t::check_pre_breakpoint(cpu_state *cpu, StopHit *hit_out) {
    uint32_t fullpc = cpu->full_pc;
    if (stepover_bp && (fullpc == stepover_bp)) {
        stepover_bp = 0;
        if (hit_out) {
            *hit_out = StopHit{STOP_STEP, 0, 0, 0, fullpc, 0, 0};
        }
        return true;
    }

    if (computer && computer->breakpoints) {
        auto hit = computer->breakpoints->check_pre(cpu);
        if (hit) {
            if (hit_out) {
                *hit_out = *hit;
            }
            return true;
        }
    }
    return false;
}

bool debug_window_t::check_post_breakpoint(cpu_state *cpu, system_trace_entry_t *entry, StopHit *hit_out) {
    if (step_out_active) {
        if ((entry->opcode == 0x60) || (entry->opcode == 0x6B)) {
            step_out_active = false;
            if (hit_out) {
                uint32_t pc = (static_cast<uint32_t>(entry->pb) << 16) | entry->pc;
                *hit_out = StopHit{STOP_STEP, 0, 0, 0, pc, entry->eaddr,
                                   static_cast<uint32_t>(entry->data & 0xFF)};
            }
            return true;
        }
    }

    if (computer && computer->breakpoints) {
        auto hit = computer->breakpoints->check_post(cpu, entry);
        if (hit) {
            if (hit_out) {
                *hit_out = *hit;
            }
            return true;
        }
    }
    return false;
}

bool debug_window_t::needs_breakpoint_checks() const {
    if (window_open) {
        return true;
    }
    if (stepover_bp || step_out_active) {
        return true;
    }
    if (computer && computer->breakpoints && computer->breakpoints->has_enabled()) {
        return true;
    }
    return false;
}

void debug_window_t::set_mmu(MMU *mmu) {
    this->mmu = mmu;
    monitor_.bind(mmu, &memory_watches, computer->breakpoints, disasm, &debug_displays,
                  cpu ? cpu->trace_buffer : nullptr);
}

void debug_window_t::execute_command(const std::string& command) {
    int num_mem_watches = memory_watches.size();
    int num_debug_displays = debug_displays.size();

    monitor_.bind(mmu, &memory_watches, computer->breakpoints, disasm, &debug_displays, cpu->trace_buffer);
    const auto &output = monitor_.execute(command);

    mon_history.push_back(command); // put into the scrollback
    if (mon_history.size() > 10) {
        mon_history.erase(mon_history.begin());
    }
    mon_history_position = mon_history.size(); // one past the end of the list

    mon_display_buffer.push_back("> " + command);

    for (const auto &line : output) {
        std::cout << line << std::endl;
        mon_display_buffer.push_back(line);
    }
    // Stick to live end when already there; otherwise keep scroll offset (clamped on sync).
    if (mon_view_position == 0 && mon_scroll_) {
        mon_scroll_->set_position(0);
    }
    mon_textinput->clear_edit();

    if (num_mem_watches != memory_watches.size()) {
        // memory watch list changed, so we need to re-render the memory pane
        if (memory_watches.size() > 0) {
            set_panel_visible(DEBUG_PANEL_MEMORY, true);
        }
    }
    if (num_debug_displays != debug_displays.size()) {
        // debug display list changed, so we need to re-render the debug pane
        if (debug_displays.size() > 0) {
            set_panel_visible(DEBUG_PANEL_MEMORY, true);
        }
    }
}

void debug_window_t::separator_line(debug_panel_t pane, int y) {
    int x = pane_area[pane].x;
    int w = pane_area[pane].w;
    ui_ctx.line(x, (y*font_line_height)-3, x+w, (y*font_line_height)-3, 0xFFFFFFFF);
}

void debug_window_t::draw_text(debug_panel_t pane, int x,int y, const char *textToShow) {
    text_renderer->render(textToShow, window_margin + pane_area[pane].x, y*font_line_height);
}

bool debug_window_t::is_pane_first(debug_panel_t pane) const {
    for (int i = 0; i < pane; i++) {
        if (panel_visible[i]) {
            return false;
        }
    }
    return true;
}
int debug_window_t::num_lines_in_pane(debug_panel_t pane) {
    int pane_is_first = is_pane_first(pane);
    return (window_height - (pane_is_first ? control_area_height : 0)) / font_line_height;
}

/*
view_size - how big is the view area
disasm_lines - how many lines of forward disassembly to show

data_size =    
    number of trace buffer lines
    plus
    10 (for forward disassembly)



*/
void debug_window_t::render_pane_trace() {
    char buffer[256];
    int x = 0;
    int w = pane_area[DEBUG_PANEL_TRACE].w;

    bool single_step = computer->execution_mode == EXEC_STEP_INTO;

    int view_size = (window_height - control_area_height) / font_line_height;
    size_t trace_head = cpu->trace_buffer->head;
    size_t trace_size = cpu->trace_buffer->size;
    size_t trace_count = cpu->trace_buffer->count;
    
    // if we're single-stepping, we need to leave room for proactive disassembly
    constexpr size_t disasm_line_count = kTraceDisasmLineCount;
    size_t start_idx;

    size_t disasm_displayed;
    size_t trace_displayed;
    if (single_step) {
        // THIS WORKS but is complex.
        if (trace_count < view_size-disasm_line_count) {
            trace_displayed = trace_count;
            start_idx = cpu->trace_buffer->tail;
            disasm_displayed = disasm_line_count;
        } else if (view_position < (int)disasm_line_count) {
            disasm_displayed = disasm_line_count - view_position;
            trace_displayed = view_size - disasm_displayed;
            start_idx = (trace_head - trace_displayed ) % trace_size;
        } else {
            disasm_displayed = 0;
            trace_displayed = view_size - disasm_displayed;
            start_idx = (trace_head - view_size - view_position + disasm_line_count) % trace_size;
        }
    } else {
        disasm_displayed = 0;
        trace_displayed = view_size;
        start_idx = (trace_head - view_position) % trace_size;
    }
    
    // Calculate the starting index to show the last numlines entries
    // We need to handle wrapping around the circular buffer

    // convert position (0 being at the very end of data) to index into trace buffer.
    // if position = 0, then it's data_size + disasm_lines - view_size
    // position = 1, then it's data_size + disasm_lines - view_size + 1   
    // when the idx gets to trace_head, then we need to switch to show forward disassembly.
    // do numlines minus 10, to leave room for 10 lines of prospective disassembly

    for (int i = 0; i < trace_displayed; i++) {
        size_t idx = (start_idx + i) % trace_size;
        char *line = cpu->trace_buffer->decode_trace_entry(&cpu->trace_buffer->entries[idx]);
        draw_text(DEBUG_PANEL_TRACE, x, 8 + i, line);
    }
    if (disasm_displayed) {
        step_disasm->setLinePrepend(cpu->cpu_type == PROCESSOR_65816 ? 35 : 34);
        step_disasm->setAddress(cpu->full_pc);
        std::vector<std::string> disasm_lines = step_disasm->disassemble(disasm_displayed);
        // first line of disassembly is current unexecuted instruction. highlight the background. in white.
        
        SDL_FRect hl_rect = {0, (float)(8 + trace_displayed)*font_line_height, 750, (float)font_line_height};
        ui_ctx.fill_rect(hl_rect, 0x606060FF);
        text_renderer->set_color(0, 255, 255, 255);
        for (int i = 0; i < disasm_lines.size(); i++) {
            draw_text(DEBUG_PANEL_TRACE, x, 8 + trace_displayed + i, disasm_lines[i].c_str());
            //printf("disasm_lines[%d] = %s\n", i, disasm_lines[i].c_str());
        }
    }
    text_renderer->set_color(255, 255, 255, 255);
    separator_line(DEBUG_PANEL_TRACE, 3);
    snprintf(buffer, sizeof(buffer),
             "T)race: %s  SPACE: Step  RETURN: Run  Up/Dn/PgUp/PgDn/Home/End",
             cpu->trace ? "ON " : "OFF");
    draw_text(DEBUG_PANEL_TRACE, x, 3, buffer);
  
    separator_line(DEBUG_PANEL_TRACE, 4);

    // TODO: do this better by using functions to emit items into a buffer tracking 'cursor'
    if (cpu->cpu_type == PROCESSOR_65816) {
        draw_text(DEBUG_PANEL_TRACE, x, 4, "PB/PC   DB DP      A    X    Y  SP    N V M XB D I Z C  e  IRQ");
        if ((cpu->_M == 1) && (cpu->_X == 1)) {
            snprintf(buffer, sizeof(buffer), "%02X/%04X %02X %04X   %02X   %02X   %02X %04X   %d %d %d  %d %d %d %d %d  %d  %d", 
                cpu->pb, cpu->pc, cpu->db, cpu->d, cpu->a & 0xFF, cpu->x & 0xFF, cpu->y & 0xFF, cpu->sp, 
                cpu->N, cpu->V, cpu->_M, cpu->_X, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->E, cpu->irq_asserted);
        } else if ((cpu->_M == 1) && (cpu->_X == 0)) {
            snprintf(buffer, sizeof(buffer), "%02X/%04X %02X %04X   %02X %04X %04X %04X   %d %d %d  %d %d %d %d %d  %d  %d", 
                cpu->pb, cpu->pc, cpu->db, cpu->d, cpu->a & 0xFF, cpu->x, cpu->y, cpu->sp, 
                cpu->N, cpu->V, cpu->_M, cpu->_X, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->E, cpu->irq_asserted);
        } else if ((cpu->_M == 0) && (cpu->_X == 1)) {
            snprintf(buffer, sizeof(buffer), "%02X/%04X %02X %04X %04X   %02X   %02X %04X   %d %d %d  %d %d %d %d %d  %d  %d", 
                cpu->pb, cpu->pc, cpu->db, cpu->d, cpu->a, cpu->x & 0xFF, cpu->y & 0xFF, cpu->sp, 
                cpu->N, cpu->V, cpu->_M, cpu->_X, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->E, cpu->irq_asserted);
        } else {
            snprintf(buffer, sizeof(buffer), "%02X/%04X %02X %04X %04X %04X %04X %04X   %d %d %d  %d %d %d %d %d  %d  %d", 
                cpu->pb, cpu->pc, cpu->db, cpu->d, cpu->a, cpu->x, cpu->y, cpu->sp, 
                cpu->N, cpu->V, cpu->_M, cpu->_X, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->E, cpu->irq_asserted);
        }
    } else {
        draw_text(DEBUG_PANEL_TRACE, x, 4, "PC     A  X  Y  SP     N V - B D I Z C    e  IRQ");
        snprintf(buffer, sizeof(buffer), "%04X   %02X %02X %02X %04X   %d %d - %d %d %d %d %d  %d  %d", cpu->pc, cpu->a, cpu->x, cpu->y, cpu->sp, 
        cpu->N, cpu->V, cpu->B, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->E, cpu->irq_asserted);
    }
    //draw_text(DEBUG_PANEL_TRACE, x, 4, "PC     A  X  Y  SP     N V - D B I Z C  e  IRQ");

    draw_text(DEBUG_PANEL_TRACE, x, 5, buffer);

    // Todo: Replace this? nah..
    //snprintf(buffer, sizeof(buffer), "Cycles: %18llu    MHz: %10.5f", cpu->cycles, cpu->e_mhz);
    //draw_text(DEBUG_PANEL_TRACE, x, 6, buffer);

    separator_line(DEBUG_PANEL_TRACE, 7);
    if (cpu->cpu_type == PROCESSOR_65816) {
        draw_text(DEBUG_PANEL_TRACE, x, 7, "   Cycle      A    X    Y  SP   P     PC                                Eff  M");
    } else {
        draw_text(DEBUG_PANEL_TRACE, x, 7, "   Cycle      A  X  Y  SP   P      PC                                Eff  M");
    }
    
    separator_line(DEBUG_PANEL_TRACE, 8);
    ui_ctx.line(w + x -1.0f, 0, w + x -1.0f, window_height, 0xFFFFFFFF);

    sync_trace_scrollbar();
    if (trace_scroll_) {
        trace_scroll_->render();
    }
}

void debug_window_t::sync_trace_scrollbar() {
    if (!trace_scroll_ || !cpu || !cpu->trace_buffer) {
        return;
    }

    const bool single_step = computer->execution_mode == EXEC_STEP_INTO;
    const int trace_count = static_cast<int>(cpu->trace_buffer->count);
    const int view_size = (window_height - control_area_height) / font_line_height;
    const int content_size =
        single_step ? (trace_count + static_cast<int>(kTraceDisasmLineCount)) : trace_count;
    const int page_size = std::max(1, view_size);

    const int x = pane_area[DEBUG_PANEL_TRACE].x;
    const int w = pane_area[DEBUG_PANEL_TRACE].w;
    trace_scroll_->set_visible(panel_visible[DEBUG_PANEL_TRACE] != 0);
    trace_scroll_->set_position(static_cast<float>(x + w - 10), static_cast<float>(control_area_height));
    trace_scroll_->size(10.0f, static_cast<float>(window_height - control_area_height));
    trace_scroll_->set_range(content_size, page_size);
    trace_scroll_->set_position(view_position);
    view_position = trace_scroll_->position();
}

void debug_window_t::apply_trace_scroll_position() {
    if (trace_scroll_) {
        view_position = trace_scroll_->position();
    }
}

void debug_window_t::monitor_layout_metrics(int &base_line, int &buf_area_lines, int &textarea_pos) const {
    textarea_pos = (window_height / font_line_height) - 1;
    if (is_pane_first(DEBUG_PANEL_MONITOR)) { // make room for buttons at the top
        base_line = 3;
        buf_area_lines = textarea_pos - 3;
    } else {
        base_line = 0;
        buf_area_lines = textarea_pos;
    }
    if (buf_area_lines < 1) {
        buf_area_lines = 1;
    }
}

void debug_window_t::sync_monitor_scrollbar() {
    if (!mon_scroll_) {
        return;
    }

    int base_line = 0;
    int buf_area_lines = 1;
    int textarea_pos = 0;
    monitor_layout_metrics(base_line, buf_area_lines, textarea_pos);

    const int x = pane_area[DEBUG_PANEL_MONITOR].x;
    const int w = pane_area[DEBUG_PANEL_MONITOR].w;
    const float bar_y = static_cast<float>(base_line * font_line_height);
    const float bar_h = static_cast<float>((textarea_pos - base_line) * font_line_height);

    mon_scroll_->set_visible(panel_visible[DEBUG_PANEL_MONITOR] != 0);
    mon_scroll_->set_position(static_cast<float>(x + w - 10), bar_y);
    mon_scroll_->size(10.0f, std::max(1.0f, bar_h));
    mon_scroll_->set_range(static_cast<int>(mon_display_buffer.size()), buf_area_lines);
    mon_scroll_->set_position(mon_view_position);
    mon_view_position = mon_scroll_->position();
}

void debug_window_t::apply_monitor_scroll_position() {
    if (mon_scroll_) {
        mon_view_position = mon_scroll_->position();
    }
}

void debug_window_t::mon_scroll_by(int lines) {
    sync_monitor_scrollbar();
    if (mon_scroll_) {
        mon_scroll_->scroll_by(lines);
        apply_monitor_scroll_position();
    }
}

bool debug_window_t::point_in_panel(debug_panel_t pane, float px, float py) const {
    if (!panel_visible[pane]) {
        return false;
    }
    const SDL_Rect &r = pane_area[pane];
    return px >= r.x && px < r.x + r.w && py >= 0 && py < window_height;
}

void debug_window_t::render_pane_monitor() {

    if (!panel_visible[DEBUG_PANEL_MONITOR]) {
        return;
    }

    int x = pane_area[DEBUG_PANEL_MONITOR].x;
    int base_line = 0;
    int buf_area_lines = 1;
    int textarea_pos = 0;
    monitor_layout_metrics(base_line, buf_area_lines, textarea_pos);

    sync_monitor_scrollbar();

    separator_line(DEBUG_PANEL_MONITOR, textarea_pos);
    draw_text(DEBUG_PANEL_MONITOR, x, textarea_pos, ">");
    mon_textinput->set_position(x + 20, (textarea_pos * font_line_height));
    mon_textinput->render();

    const int bufferlines = static_cast<int>(mon_display_buffer.size());
    int startline = 0;
    int dolines = 0;
    if (bufferlines > buf_area_lines) {
        startline = std::max(0, bufferlines - buf_area_lines - mon_view_position);
        dolines = buf_area_lines;
        if (startline + dolines > bufferlines) {
            dolines = bufferlines - startline;
        }
    } else {
        dolines = bufferlines;
    }

    for (int i = 0; i < dolines; i++) {
        draw_text(DEBUG_PANEL_MONITOR, x, base_line + i, mon_display_buffer[i + startline].c_str());
    }

    if (mon_scroll_) {
        mon_scroll_->render();
    }
}

void debug_window_t::event_pane_monitor(SDL_Event &event) {
    mon_textinput->handle_mouse_event(event);
}

void debug_window_t::render_pane_devices() {
    // Disk II Devices - get motor on/off, track number, slot and drive

    // Unidisk - last block read/written, slot and drive

    // Memory - call this II+ MMU; show memory map: what slot (if any) is holding C800-CFFF; languagecard status read/write ram/rom and bank (1 or 2).
    // this one is composing from a couple different sources. Who is responsible for this? mmu for C800; languagecard for D000-FFFF
    if (!panel_visible[DEBUG_PANEL_DEVICES]) {
        return;
    }

    int x = pane_area[DEBUG_PANEL_DEVICES].x;
    int y = pane_area[DEBUG_PANEL_DEVICES].y;
    int base_line = 0;
    
}


int debug_window_t::memory_pane_base_line() const {
    // Reserve space for the debug-display toggle strip at the top of Watch.
    if (!debug_display_container || !debug_display_container->is_visible()) {
        return is_pane_first(DEBUG_PANEL_MEMORY) ? 3 : 0;
    }
    float x = 0, y = 0, w = 0, h = 0;
    debug_display_container->get_tile_position(x, y);
    debug_display_container->get_tile_size(&w, &h);
    (void)x;
    (void)w;
    const int bottom = static_cast<int>(y + h) + 4;
    return (bottom + font_line_height - 1) / font_line_height;
}

void debug_window_t::toggle_debug_display(const std::string &name) {
    auto it = std::find(debug_displays.begin(), debug_displays.end(), name);
    if (it != debug_displays.end()) {
        debug_displays.erase(it);
    } else {
        debug_displays.push_back(name);
    }
}

void debug_window_t::sync_debug_display_buttons() {
    if (!debug_display_container || !computer) {
        return;
    }

    const auto &handlers = computer->debug_display_handlers;
    const size_t handler_count = handlers.size();

    if (debug_display_container->count() != handler_count) {
        // remove_all_tiles() does not delete; free tiles ourselves before rebuild.
        std::vector<Tile_t *> old_tiles = debug_display_container->get_tiles();
        for (Tile_t *tile : old_tiles) {
            delete tile;
        }
        debug_display_container->remove_all_tiles();

        Style_t SS;
        SS.background_color = 0x00426340;
        SS.border_color = 0xFFFFFFFF;
        SS.hover_color = 0x606060FF;
        SS.text_color = 0xFFFFFFFF;
        SS.padding = 4;
        SS.border_width = 1;

        for (const auto &handler : handlers) {
            const std::string name = handler.name;
            Button_t *btn = new Button_t(&ui_ctx, name, SS);
            const int text_w = text_renderer->string_width(name);
            btn->size(static_cast<float>(std::max(text_w + 16, 50)), 22.0f);
            btn->on_click([this, name](const SDL_Event &event) -> bool {
                (void)event;
                toggle_debug_display(name);
                return true;
            });
            debug_display_container->add(btn);
        }
    }

    for (size_t i = 0; i < handler_count; i++) {
        Button_t *btn = static_cast<Button_t *>(debug_display_container->get_tile(i));
        if (!btn) {
            continue;
        }
        const bool active = std::find(debug_displays.begin(), debug_displays.end(), handlers[i].name)
                            != debug_displays.end();
        btn->style.background_color = active ? 0x00A1F0FF : 0x00426340;
    }
}

void debug_window_t::layout_debug_display_container() {
    if (!debug_display_container) {
        return;
    }

    const bool watch_visible = panel_visible[DEBUG_PANEL_MEMORY] != 0;
    debug_display_container->set_visible(watch_visible);
    if (!watch_visible) {
        return;
    }

    const int x = pane_area[DEBUG_PANEL_MEMORY].x + 10;
    // Sit below the pane-toggle tabs when Watch is the leftmost pane.
    const int y = is_pane_first(DEBUG_PANEL_MEMORY) ? 50 : 15;
    const float pane_w = static_cast<float>(pane_area[DEBUG_PANEL_MEMORY].w - 20);

    // Width is fixed to the Watch pane; WrapContainer_t::layout sets height to fit.
    debug_display_container->set_position(static_cast<float>(x), static_cast<float>(y));
    debug_display_container->size(pane_w, 1.0f);
    debug_display_container->layout();
}

void debug_window_t::render_pane_memory() {

    if (!panel_visible[DEBUG_PANEL_MEMORY]) {
        return;
    }

    // just for testing, display the text page memory.
    int x = pane_area[DEBUG_PANEL_MEMORY].x;
    int y = pane_area[DEBUG_PANEL_MEMORY].y;
    int base_line = memory_pane_base_line();

    char buffer[256] = {' '};

    int index = 0; // number of byte on current line being displayed
    int line = 0; // vertical line number to output text to on display
    char *ptr = buffer;

    for (MemoryWatch::iterator watch = memory_watches.begin(); watch != memory_watches.end(); ++watch) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (uint32_t i = watch->start; i <= watch->end; i++) {
            if (index == 0) {
                decode_hex_addr(buffer, i);
                ptr += 7; // BB/AAAA
                *ptr++ = ':';
                *ptr++ = ' ';
            }
            uint8_t mem = mmu->read(i);
            decode_hex_byte( ptr, mem);
            decode_ascii(buffer+56+index, mem);
            ptr+=2;
            *ptr++ = ' ';
            index++;
            if (index == 16) {
                buffer[72] = 0;
                draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
                memset(buffer, ' ', sizeof(buffer));
                ptr = buffer;
                index = 0;
                line++;
            }
        }
        // draw anything left over
        if (index > 0 && index < 16) {
            buffer[72] = 0;
            draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
            memset(buffer, ' ', sizeof(buffer));
            line++;
        }
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        separator_line(DEBUG_PANEL_MEMORY, base_line + line);
        ptr = buffer;
        index = 0;
    }

    for (auto &display : debug_displays) {
        separator_line(DEBUG_PANEL_MEMORY, base_line + line);
        draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, display.c_str());
        line++;
        auto debug_handler = computer->call_debug_display_handler(display);
        if (debug_handler) {
            const std::vector<std::string>& lines = debug_handler->getLines();
            for (int i = 0; i < lines.size(); i++) {
                draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, lines[i].c_str());
                line++;
            }
            delete debug_handler;
        }
    }

}

void debug_window_t::render() {
    char buffer[256];

    if (!window_open) {
        return;
    }

    // update - I feel like this should be in handle_event, or in a different method altogether.
    // Update button states based on current tab
    for (size_t i = 0; i < DEBUG_PANEL_COUNT; i++) {
        Button_t* btn = static_cast<Button_t*>(tab_container->get_tile(i));
        if (btn) {
            // Set active tab button to different background color
            if (panel_visible[i]) {
                btn->style.background_color = 0x00A1F0FF; // Active tab color
            } else {
                btn->style.background_color = 0x00426340; // Inactive tab color
            }
        }
    }

    // end of update

    // Step controls / scrollbar belong to the Trace pane; hide them when Trace is off
    // so they don't draw into whatever pane is to the right.
    step_container->set_visible(panel_visible[DEBUG_PANEL_TRACE] != 0);
    if (trace_scroll_) {
        trace_scroll_->set_visible(panel_visible[DEBUG_PANEL_TRACE] != 0);
    }
    if (mon_scroll_) {
        mon_scroll_->set_visible(panel_visible[DEBUG_PANEL_MONITOR] != 0);
    }

    // Watch-pane debug-display toggles (handlers register after debugger construction).
    sync_debug_display_buttons();
    layout_debug_display_container();

    ui_ctx.color(0x000000FF);
    SDL_RenderClear(renderer);

    for (Container_t *container : containers) {
        container->render();
    }

    ui_ctx.color(0xFFFFFFFF);
    text_renderer->set_color(255, 255, 255, 255);

    if (panel_visible[DEBUG_PANEL_TRACE]) {
        render_pane_trace();
    }
    if (panel_visible[DEBUG_PANEL_MONITOR]) {
        render_pane_monitor();
    }
    if (panel_visible[DEBUG_PANEL_MEMORY]) {
        render_pane_memory();
    }

    SDL_RenderPresent(renderer);
}

void debug_window_t::resize(int width, int height) {
    window_width = width;
    window_height = height;
    lines_in_view_area = (window_height - control_area_height) / font_line_height;
}

bool debug_window_t::handle_pane_event_monitor(SDL_Event &event) {
    if (event.type == SDL_EVENT_KEY_DOWN && mon_textinput->is_edit_active()) {
        int base_line = 0;
        int buf_area_lines = 1;
        int textarea_pos = 0;
        monitor_layout_metrics(base_line, buf_area_lines, textarea_pos);

        if (event.key.key == SDLK_UP) {
            if (mon_history.size() > 0) {
                mon_history_position--;
                if (mon_history_position < 0) {
                    mon_history_position = mon_history.size() - 1;
                }
                if (mon_history_position < 0) mon_history_position = 0;
                mon_textinput->set_text(mon_history[mon_history_position]);
                mon_textinput->set_cursor_position(mon_textinput->get_text().size());
            }
            return true;
        }
        if (event.key.key == SDLK_DOWN) {
            if (mon_history.size() > 0) {
                mon_history_position++;
                if (mon_history_position >= mon_history.size()) {
                    mon_history_position = mon_history.size() - 1;
                }
                mon_textinput->set_text(mon_history[mon_history_position]);
                mon_textinput->set_cursor_position(mon_textinput->get_text().size());
            }
            return true;
        }
        if (event.key.key == SDLK_PAGEUP) {
            mon_scroll_by(buf_area_lines);
            return true;
        }
        if (event.key.key == SDLK_PAGEDOWN) {
            mon_scroll_by(-buf_area_lines);
            return true;
        }
        if (event.key.key == SDLK_HOME) {
            sync_monitor_scrollbar();
            if (mon_scroll_) {
                mon_scroll_->scroll_to_home();
                apply_monitor_scroll_position();
            }
            return true;
        }
        if (event.key.key == SDLK_END) {
            sync_monitor_scrollbar();
            if (mon_scroll_) {
                mon_scroll_->scroll_to_end();
                apply_monitor_scroll_position();
            }
            return true;
        }
    }
    if (mon_textinput->handle_mouse_event(event)) {
        return true;
    }
    return false;
}

void debug_window_t::step_one() {
    computer->execution_mode = EXEC_STEP_INTO;
    computer->instructions_left = 1;
    stepover_bp = 0; // clear for good measure
}
void debug_window_t::resume() {
    computer->execution_mode = EXEC_NORMAL;
    computer->instructions_left = 0;
    view_position = 0;
    stepover_bp = 0; // clear for good measure
}
void debug_window_t::step_over() {
    if (computer->execution_mode != EXEC_STEP_INTO) return; // if not in step ignore
    stepover_bp = (cpu->pb << 16) | cpu->pc;
    // trace has not happened yet!
    uint8_t opcode = mmu->read(stepover_bp);
    if ((opcode == 0x20) || (opcode == 0xFC)) {
        stepover_bp += 3;
    } else if (opcode == 0x22) {
        stepover_bp += 4;
    } else { // to "step over" a non-subroutine call it's just one more instruction.
        computer->instructions_left = 1;
        return;
    }
    printf("Step over BP: %06X\n", stepover_bp);
    computer->execution_mode = EXEC_NORMAL;
}
void debug_window_t::step_out() {
    if (computer->execution_mode != EXEC_STEP_INTO) return; // if not in step ignore
    step_out_active = true;
    computer->execution_mode = EXEC_NORMAL;
}
void debug_window_t::trace_scroll_up(int lines) {
    sync_trace_scrollbar();
    if (trace_scroll_) {
        trace_scroll_->scroll_by(lines);
        apply_trace_scroll_position();
    } else {
        view_position += lines;
    }
}
void debug_window_t::trace_scroll_down(int lines) {
    sync_trace_scrollbar();
    if (trace_scroll_) {
        trace_scroll_->scroll_by(-lines);
        apply_trace_scroll_position();
    } else {
        view_position -= lines;
        if (view_position < 0) {
            view_position = 0;
        }
    }
}
void debug_window_t::trace_scroll(float y) {
    if (y < 0 && y > -1) y = -1;
    if (y > 0 && y < 1) y = 1;
    sync_trace_scrollbar();
    if (trace_scroll_) {
        trace_scroll_->scroll_by(static_cast<int>(y));
        apply_trace_scroll_position();
    } else {
        view_position += static_cast<int>(y);
        if (view_position < 0) {
            view_position = 0;
        }
    }
}
bool debug_window_t::handle_event(SDL_Event &event) {
#if defined(__EMSCRIPTEN__)
    // Emscripten supports only one window, so the debugger has no separate OS
    // window of its own. Its window_id collides with the main window's events
    // (and with focusless windowID==0 events), which would swallow every
    // keystroke meant for the emulator. The debugger isn't usable on the web
    // anyway, so disable its event handling entirely here.
    (void)event;
    return false;
#else
    // If the debugger's own OS window doesn't exist, never claim events.
    if (!window || window_id == 0) {
        return false;
    }
    if (event.window.windowID == window_id) {
        if (handle_pane_event_monitor(event)) {
            return true;
        }
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (computer->execution_mode == EXEC_STEP_INTO) { // turn off step mode if they close the window.
                    computer->execution_mode = EXEC_NORMAL;
                    computer->instructions_left = 0;
                }
                set_closed();
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                resize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float mx = 0, my = 0;
                SDL_GetMouseState(&mx, &my);
                float wy = event.wheel.y;
                if (wy < 0 && wy > -1) wy = -1;
                if (wy > 0 && wy < 1) wy = 1;
                if (point_in_panel(DEBUG_PANEL_MONITOR, mx, my)) {
                    mon_scroll_by(static_cast<int>(wy));
                } else if (panel_visible[DEBUG_PANEL_TRACE]) {
                    trace_scroll(event.wheel.y);
                }
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                const bool mod_nav = (event.key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)) != 0;
                switch (event.key.key) {
                    case SDLK_SPACE: step_one(); break;
                    case SDLK_F10: 
                        if (window_open) {
                            set_closed();
                            resume();
                        } else {
                            set_open();
                        }
                        break;
                    case SDLK_RETURN: resume(); break;
                    case SDLK_O: step_over(); break;
                    case SDLK_R: step_out(); break;
                    case SDLK_UP:
                        if (mod_nav) {
                            trace_scroll_up(lines_in_view_area);
                        } else {
                            trace_scroll_up(1);
                        }
                        break;
                    case SDLK_DOWN:
                        if (mod_nav) {
                            trace_scroll_down(lines_in_view_area);
                        } else {
                            trace_scroll_down(1);
                        }
                        break;
                    case SDLK_LEFT:
                        if (mod_nav) {
                            sync_trace_scrollbar();
                            if (trace_scroll_) {
                                trace_scroll_->scroll_to_home();
                                apply_trace_scroll_position();
                            }
                        }
                        break;
                    case SDLK_RIGHT:
                        if (mod_nav) {
                            sync_trace_scrollbar();
                            if (trace_scroll_) {
                                trace_scroll_->scroll_to_end();
                                apply_trace_scroll_position();
                            }
                        }
                        break;
                    case SDLK_PAGEUP: trace_scroll_up(lines_in_view_area); break;
                    case SDLK_PAGEDOWN: trace_scroll_down(lines_in_view_area); break;
                    case SDLK_HOME:
                        sync_trace_scrollbar();
                        if (trace_scroll_) {
                            trace_scroll_->scroll_to_home();
                            apply_trace_scroll_position();
                        }
                        break;
                    case SDLK_END:
                        sync_trace_scrollbar();
                        if (trace_scroll_) {
                            trace_scroll_->scroll_to_end();
                            apply_trace_scroll_position();
                        }
                        break;
                    case SDLK_T: cpu->trace = !cpu->trace; break;
                }
                return true; // consume the key down.
            }
            default:
                if (mon_scroll_ && panel_visible[DEBUG_PANEL_MONITOR]
                    && mon_scroll_->handle_mouse_event(event)) {
                    break;
                }
                if (trace_scroll_ && panel_visible[DEBUG_PANEL_TRACE]
                    && trace_scroll_->handle_mouse_event(event)) {
                    break;
                }
                for (Container_t *container : containers) {
                    container->handle_mouse_event(event);
                }
                break;
        }
        return true;
    } else {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10) {
            if (window_open) {
                set_closed();
                resume();
            } else {
                set_open();
            }
        }
    }
    return false;
#endif
}

bool debug_window_t::is_open() {
    return window_open;
}

void debug_window_t::set_open() {
    disasm = new Disassembler(mmu, cpu->cpu_type); // used in monitor pane
    step_disasm = new Disassembler(mmu, cpu->cpu_type); // used in trace pane
    monitor_.bind(mmu, &memory_watches, computer->breakpoints, disasm, &debug_displays, cpu->trace_buffer);
    window_open = true;
    computer->video_system->show(window);
    computer->video_system->raise(window);
}

void debug_window_t::set_closed() {
    window_open = false;

    computer->video_system->hide(window);
    computer->video_system->raise(computer->video_system->window); // TODO: awkward.

    delete disasm;
    disasm = nullptr;
    delete step_disasm;
    step_disasm = nullptr;
    monitor_.bind(mmu, &memory_watches, computer->breakpoints, nullptr, &debug_displays, cpu->trace_buffer);
}

void debug_window_t::resize_window() {
    constexpr int TRACE_WIDTH = 765;

    window_width = 0;
    if (panel_visible[DEBUG_PANEL_TRACE]) {
        pane_area[DEBUG_PANEL_TRACE].x = window_width;
        pane_area[DEBUG_PANEL_TRACE].w = TRACE_WIDTH;
        window_width += TRACE_WIDTH;
    }    
    if (panel_visible[DEBUG_PANEL_MONITOR]) {
        pane_area[DEBUG_PANEL_MONITOR].x = window_width;
        pane_area[DEBUG_PANEL_MONITOR].w = 680;
        window_width += 680;
    }
    if (panel_visible[DEBUG_PANEL_MEMORY]) {
        pane_area[DEBUG_PANEL_MEMORY].x = window_width;
        pane_area[DEBUG_PANEL_MEMORY].w = 640;
        window_width += 640;
    }
    if (window_width < TRACE_WIDTH) {
        window_width = TRACE_WIDTH;
    }
    SDL_SetWindowSize(window, window_width, window_height);
    layout_debug_display_container();
}

void debug_window_t::toggle_panel(debug_panel_t panel) {
    panel_visible[panel] = !panel_visible[panel];
    resize_window();
}

void debug_window_t::set_panel_visible(debug_panel_t panel, bool visible) {
    panel_visible[panel] = visible;
    resize_window();
}