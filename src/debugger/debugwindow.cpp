#include <SDL3/SDL.h>
#include <iostream>

#include "debugwindow.hpp"
#include "cpu.hpp"
#include "debugger/trace.hpp"
#include "platforms.hpp"
#include "util/TextRenderer.hpp"
#include "util/HexDecode.hpp"
#include "computer.hpp"
#include "videosystem.hpp"
#include "ui/Container.hpp"
#include "ui/Button.hpp"
#include "ui/TextInput.hpp"
#include "debugger/ExecuteCommand.hpp"
#include "debugger/MonitorCommand.hpp"
#include "debugger/disasm.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "mmus/mmu_iie.hpp"
#include "devices/mockingboard/mb.hpp"

debug_window_t::debug_window_t(computer_t *computer) {
    this->computer = computer;
    this->cpu = computer->cpu;

    panel_visible[DEBUG_PANEL_TRACE] = 1; // all default to off, so enable here.

    // create a new window
    window = SDL_CreateWindow("GSSquared Debugger", window_width, window_height, SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIDDEN);
    // create a new renderer
    renderer = SDL_CreateRenderer(window, nullptr);

    text_renderer = new TextRenderer(renderer, "/fonts/OxygenMono-Regular.ttf", 15.0f);
    text_renderer->set_color(255, 255, 255, 255);
    font_line_height = text_renderer->font_line_height;

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

    tab_container = new Container_t(renderer, 10, CS);  // Increased to 5 to accommodate the mouse position tile
    tab_container->set_position(25, 15);
    tab_container->set_tile_size(400, 35);
    containers.push_back(tab_container);

    Style_t SS;
    //SS.background_color = 0x00A1F0FF; // active (20% brighter)
    SS.background_color = 0x00426340; // 50% darker than 0x0084C6FF
    SS.border_color = 0xFFFFFFFF;
    SS.hover_color = 0x606060FF;
    SS.text_color = 0xFFFFFFFF;
    SS.padding = 4;
    SS.border_width = 1;
    
    Button_t* s1 = new Button_t("Trace", SS);
    s1->set_tile_size(60, 20);
    s1->set_text_renderer(text_renderer); // set text renderer for the button
    s1->set_click_callback([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_TRACE);
        return true;
    });
    tab_container->add_tile(s1,0);
    
    Button_t* s2 = new Button_t("Monitor", SS);
    s2->set_tile_size(60, 20);
    s2->set_text_renderer(text_renderer); // set text renderer for the button
    s2->set_click_callback([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_MONITOR);
        return true;
    });
    tab_container->add_tile(s2,1);

    Button_t* s3 = new Button_t("Memory", SS);
    s3->set_tile_size(60, 20);
    s3->set_text_renderer(text_renderer); // set text renderer for the button
    s3->set_click_callback([this](const SDL_Event& event) -> bool {
        toggle_panel(DEBUG_PANEL_MEMORY);
        return true;
    });

    tab_container->add_tile(s3,2);

    tab_container->layout();

    mon_textinput = new TextInput_t("help", SS);
    mon_textinput->set_text_renderer(text_renderer);
    mon_textinput->set_max_length(80);
    mon_textinput->set_tile_size(600, 20);
    mon_textinput->set_padding(2);
    mon_textinput->set_enter_handler([this](const SDL_Event& event) -> bool {
        // Call the command interpreter.
        execute_command(mon_textinput->get_text());
        return true;
    });
    resize_window(); // first time we need to calculate the pane locations and window size.
}

debug_window_t::~debug_window_t() {
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    delete text_renderer;
    delete tab_container;
    delete mon_textinput;
    if (disasm) delete disasm;
    if (step_disasm) delete step_disasm;
}

bool debug_window_t::check_breakpoint(system_trace_entry_t *entry) {
    for (MemoryWatch::iterator watch = breaks.begin(); watch != breaks.end(); ++watch) {
        if ((entry->pc >= watch->start && entry->pc <= watch->end) ||
            (entry->eaddr >= watch->start && entry->eaddr <= watch->end)) {
            return true;
        }
    }
    return false;
}

void debug_window_t::execute_command(const std::string& command) {
    MonitorCommand *cmd = new MonitorCommand(command);
    cmd->print();

    int num_mem_watches = memory_watches.size();
    ExecuteCommand *exec = new ExecuteCommand(computer->mmu, cmd, &memory_watches, &breaks, disasm, &debug_displays);
    exec->execute();
    
    mon_history.push_back(command); // put into the scrollback
    if (mon_history.size() > 10) {
        mon_history.erase(mon_history.begin());
    }
    mon_history_position = mon_history.size(); // one past the end of the list

    mon_display_buffer.push_back("> " + command);

    // Print the output buffer to stdout (you can remove this or redirect as needed)
    const auto& output = exec->getOutput();
    for (const auto& line : output) {
        std::cout << line << std::endl;
        mon_display_buffer.push_back(line);
    }
    mon_textinput->clear_edit();
    delete exec;
    delete cmd;

    if (num_mem_watches != memory_watches.size()) {
        // memory watch list changed, so we need to re-render the memory pane
        if (memory_watches.size() > 0) {
            set_panel_visible(DEBUG_PANEL_MEMORY, true);
        }
    }
}

void debug_window_t::separator_line(debug_panel_t pane, int y) {
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    int x = pane_area[pane].x;
    int w = pane_area[pane].w;
    SDL_RenderLine(renderer, x, (y*font_line_height)-3, x+w, (y*font_line_height)-3);
}

void debug_window_t::draw_text(debug_panel_t pane, int x,int y, const char *textToShow) {
    text_renderer->render(textToShow, window_margin + pane_area[pane].x, y*font_line_height);
}

bool debug_window_t::is_pane_first(debug_panel_t pane) {
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

void debug_window_t::render_pane_trace() {
    char buffer[256];
    int x = 0;
    int w = pane_area[DEBUG_PANEL_TRACE].w;

    bool single_step = cpu->execution_mode == EXEC_STEP_INTO;

    int numlines = (window_height - control_area_height) / font_line_height;
    size_t head = cpu->trace_buffer->head;
    size_t size = cpu->trace_buffer->size;
    
    // if we're single-stepping, we need to leave room for 10 lines of disassembly
    if (single_step) {
        numlines -= 10; // leave room for 10 lines of disassembly
    }

    // Calculate the starting index to show the last numlines entries
    // We need to handle wrapping around the circular buffer
    size_t start_idx = (head + size - numlines - view_position) % size;
    
    // do numlines minus 10, to leave room for 10 lines of prospective disassembly

    for (int i = 0; i < numlines; i++) {
        size_t idx = (start_idx + i) % size;
        char *line = cpu->trace_buffer->decode_trace_entry(&cpu->trace_buffer->entries[idx]);
        draw_text(DEBUG_PANEL_TRACE, x, 8 + i, line);
    }
    if (single_step) {
        step_disasm->setLinePrepend(36);
        step_disasm->setAddress(cpu->pc);
        std::vector<std::string> disasm_lines = step_disasm->disassemble(10);
        text_renderer->set_color(0, 255, 255, 255);
        for (int i = 0; i < disasm_lines.size(); i++) {
            draw_text(DEBUG_PANEL_TRACE, x, 8 + numlines + i, disasm_lines[i].c_str());
        }
    }
    text_renderer->set_color(255, 255, 255, 255);
    separator_line(DEBUG_PANEL_TRACE, 3);
    snprintf(buffer, sizeof(buffer), "T)race: %s  SPACE: Step  RETURN: Run  Up/Dn/PgUp/PgDn/Home/End: Scroll", cpu->trace ? "ON " : "OFF");
    draw_text(DEBUG_PANEL_TRACE, x, 3, buffer);
  
    separator_line(DEBUG_PANEL_TRACE, 4);
    draw_text(DEBUG_PANEL_TRACE, x, 4, "PC     A  X  Y  SP     N V - D B I Z C  IRQ");
    snprintf(buffer, sizeof(buffer), "%04X   %02X %02X %02X %04X   %d %d - %d %d %d %d %d   %d", cpu->pc, cpu->a, cpu->x, cpu->y, cpu->sp, 
        cpu->N, cpu->V, cpu->B, cpu->D, cpu->I, cpu->Z, cpu->C, cpu->irq_asserted!=0);
    draw_text(DEBUG_PANEL_TRACE, x, 5, buffer);

    snprintf(buffer, sizeof(buffer), "Cycles: %18llu    MHz: %10.5f", cpu->cycles, cpu->e_mhz);
    draw_text(DEBUG_PANEL_TRACE, x, 6, buffer);

    separator_line(DEBUG_PANEL_TRACE, 7);
    draw_text(DEBUG_PANEL_TRACE, x, 7, "   Cycle         A  X  Y  SP   P     PC                                Eff  M");
    separator_line(DEBUG_PANEL_TRACE, 8);

    SDL_RenderLine(renderer, w + x -1.0f, 0, w + x -1.0f, window_height);

    // draw location bar (not exactly a scrollbar)
    // Calculate number of lines entries distance of view_position is from tail
    size_t buffer_size = cpu->trace_buffer->count;
    // view_position is position of bottom line of view area. 0 = head. -size = tail basically.
    float bar_height = window_height - control_area_height;
    float bar_fraction = 1 - (float)view_position / buffer_size; // it's the inverse of the fraction since we want it to be at the bottom
    float bar_divider = (bar_fraction * bar_height);
    SDL_FRect bar_rect = {w + x - 10.0f, (float)control_area_height, 10.0f, bar_height};
    SDL_RenderRect(renderer, &bar_rect);
    bar_rect.y = control_area_height + bar_divider;
    bar_rect.h = bar_height - bar_divider;
    SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
    SDL_RenderFillRect(renderer, &bar_rect);
}

void debug_window_t::render_pane_monitor() {

    if (!panel_visible[DEBUG_PANEL_MONITOR]) {
        return;
    }

    int x = pane_area[DEBUG_PANEL_MONITOR].x;
    int y = pane_area[DEBUG_PANEL_MONITOR].y;
    int base_line = 0;
    if (is_pane_first(DEBUG_PANEL_MONITOR)) {
        base_line = 3;
    }
    int buf_area_lines = num_lines_in_pane(DEBUG_PANEL_MONITOR) - 3;
    
    separator_line(DEBUG_PANEL_MONITOR, buf_area_lines);
    char buffer[256] = {' '};
    draw_text(DEBUG_PANEL_MONITOR, x, buf_area_lines, ">");
    mon_textinput->set_tile_position(x + 20, (buf_area_lines * font_line_height));
    mon_textinput->render(renderer);

    // get number of lines in mon_display_buffer
    int bufferlines = mon_display_buffer.size();
    int startline = 0;
    int dolines = 0;
    if (bufferlines > buf_area_lines) {
        startline = bufferlines - buf_area_lines;
        dolines = buf_area_lines;
    } else dolines = bufferlines;

    for (int i = 0; i < dolines; i++) {
        draw_text(DEBUG_PANEL_MONITOR, x, base_line + i, mon_display_buffer[i+startline].c_str());
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


void debug_window_t::render_pane_memory() {

    if (!panel_visible[DEBUG_PANEL_MEMORY]) {
        return;
    }

    // just for testing, display the text page memory.
    int x = pane_area[DEBUG_PANEL_MEMORY].x;
    int y = pane_area[DEBUG_PANEL_MEMORY].y;
    int base_line = 0;
    if (is_pane_first(DEBUG_PANEL_MEMORY)) {
        base_line = 3;
    }

    char buffer[256] = {' '};

    int index = 0; // number of byte on current line being displayed
    int line = 0; // vertical line number to output text to on display
    char *ptr = buffer;

    for (MemoryWatch::iterator watch = memory_watches.begin(); watch != memory_watches.end(); ++watch) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (int i = watch->start; i <= watch->end; i++) {
            if (index == 0) {
                decode_hex_word(buffer, i);
                ptr += 4;
                *ptr++ = ':';
                *ptr++ = ' ';
            }
            uint8_t mem = computer->mmu->read(i);
            decode_hex_byte( ptr, mem);
            decode_ascii(buffer+54+index, mem);
            ptr+=2;
            *ptr++ = ' ';
            index++;
            if (index == 16) {
                buffer[70] = 0;
                draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
                memset(buffer, ' ', sizeof(buffer));
                ptr = buffer;
                index = 0;
                line++;
            }
        }
        // draw anything left over
        if (index > 0 && index < 16) {
            buffer[70] = 0;
            draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
            memset(buffer, ' ', sizeof(buffer));
            line++;
        }
        SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
        separator_line(DEBUG_PANEL_MEMORY, base_line + line);
        ptr = buffer;
        index = 0;
    }
    //
/*     display_state_t *ds = (display_state_t *)computer->get_module_state(MODULE_DISPLAY);
    if (computer->platform->id == PLATFORM_APPLE_IIE || computer->platform->id == PLATFORM_APPLE_IIE_ENHANCED) {
        iiememory_state_t *iiem = (iiememory_state_t *)computer->get_module_state(MODULE_IIEMEMORY);
        MMU_IIe *mmu = (MMU_IIe *)computer->mmu;
        if (iiem) {
            // dump the page table
            snprintf(buffer,255,"80ST: %1d RAMR: %1d RAMW: %1d ALTZP: %1d SLOTC3: %1d 80COL: %1d",
                iiem->f_80store, iiem->f_ramrd, iiem->f_ramwrt, iiem->f_altzp, mmu->f_slotc3rom, ds->f_80col);
            draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
            line++;
            snprintf(buffer,255,"INTCX: %1d ALTCHR: %1d LC [ BNK1: %1d READ: %1d WRT: %1d ]",
                mmu->f_intcxrom, ds->f_altcharset, iiem->FF_BANK_1, iiem->FF_READ_ENABLE, !iiem->_FF_WRITE_ENABLE);
            draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, buffer);
            line++;
        }
    } */

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

  /*   mb_cpu_data *mb_d = (mb_cpu_data *)computer->get_slot_state(SLOT_4);
    if (mb_d && mb_d->id == DEVICE_ID_MOCKINGBOARD) {
        separator_line(DEBUG_PANEL_MEMORY, base_line + line);
        DebugFormatter *df = debug_registers_6522(mb_d);
        const std::vector<std::string>& lines = df->getLines();
        for (int i = 0; i < lines.size(); i++) {
            draw_text(DEBUG_PANEL_MEMORY, x, base_line + line, lines[i].c_str());
            line++;
        }
        delete df;
    } */
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
                btn->set_background_color(0x00A1F0FF); // Active tab color
            } else {
                btn->set_background_color(0x00426340); // Inactive tab color
            }
        }
    }

    // end of update

    SDL_SetRenderDrawColor(renderer, 0,0,0, 255);
    SDL_RenderClear(renderer);

    for (Container_t *container : containers) {
        container->render();
    }

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
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
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (event.key.key == SDLK_UP && mon_textinput->is_edit_active()) {
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
        if (event.key.key == SDLK_DOWN && mon_textinput->is_edit_active()) {
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
    }
    if (mon_textinput->handle_mouse_event(event)) {
        return true;
    }
    return false;
}

bool debug_window_t::handle_event(SDL_Event &event) {
    if (event.window.windowID == window_id) {
        if (handle_pane_event_monitor(event)) {
            return true;
        }
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                set_closed();
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                resize(event.window.data1, event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_SPACE) {
                    cpu->execution_mode = EXEC_STEP_INTO;
                    cpu->instructions_left = 1;
                }
                if (event.key.key == SDLK_RETURN) {
                    cpu->execution_mode = EXEC_NORMAL;
                    cpu->instructions_left = 0;
                    view_position = 0;
                }
                if (event.key.key == SDLK_RETURN && event.key.mod & SDL_KMOD_CTRL) {
                    if (window_open) {
                        set_closed();
                    } else {
                        set_open();
                    }
                }
                if (event.key.key == SDLK_UP) {
                    view_position++;
                }
                if (event.key.key == SDLK_DOWN) {
                    view_position--;
                    if (view_position < 0) {
                        view_position = 0;
                    }
                }
                if (event.key.key == SDLK_PAGEUP) {
                    view_position += lines_in_view_area;
                }
                if (event.key.key == SDLK_PAGEDOWN) {
                    view_position -= lines_in_view_area;
                    if (view_position < 0) {
                        view_position = 0;
                    }
                }
                if (event.key.key == SDLK_HOME) {
                    view_position = cpu->trace_buffer->count - lines_in_view_area;
                }
                if (event.key.key == SDLK_END) {
                    view_position = 0;
                }
                if (event.key.key == SDLK_T) {
                    cpu->trace = !cpu->trace;
                }
                break;
            default:
                for (Container_t *container : containers) {
                    container->handle_mouse_event(event);
                }
                break;
        }
        return true;
    } else {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RETURN && event.key.mod & SDL_KMOD_CTRL) {
            if (window_open) {
                set_closed();
            } else {
                set_open();
            }
        }
    }
    return false;
}

bool debug_window_t::is_open() {
    return window_open;
}

void debug_window_t::set_open() {
    disasm = new Disassembler(computer->mmu);
    step_disasm = new Disassembler(computer->mmu);
    window_open = true;
    computer->video_system->show(window);
    computer->video_system->raise(window);
    //SDL_ShowWindow(window);
    //SDL_RaiseWindow(window);
}

void debug_window_t::set_closed() {
    window_open = false;

    computer->video_system->hide(window);
    computer->video_system->raise(computer->video_system->window); // TODO: awkward.

    delete disasm;
    disasm = nullptr;
    delete step_disasm;
    step_disasm = nullptr;

    // SDL_HideWindow(window);
    // SDL_RaiseWindow(video_system->window);
}

void debug_window_t::resize_window() {
   window_width = 0;
    if (panel_visible[DEBUG_PANEL_TRACE]) {
        pane_area[DEBUG_PANEL_TRACE].x = window_width;
        pane_area[DEBUG_PANEL_TRACE].w = 720;
        window_width += 720;
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
    if (window_width < 720) {
        window_width = 720;
    }
    SDL_SetWindowSize(window, window_width, window_height);
}

void debug_window_t::toggle_panel(debug_panel_t panel) {
    panel_visible[panel] = !panel_visible[panel];
    resize_window();
}

void debug_window_t::set_panel_visible(debug_panel_t panel, bool visible) {
    panel_visible[panel] = visible;
    resize_window();
}