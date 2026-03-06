

#include "TextInput.hpp"
#include "SDL3/SDL_events.h"
#include "UIContext.hpp"

TextInput_t::TextInput_t(UIContext *ctx, const std::string& text, const Style_t& style) : Tile_t(ctx, style) {
    set_text(text);
    //set_cursor_position(text.length());
    set_cursor_position(0);
}

void TextInput_t::test_truncate() {
    if (max_length > 0 && text.length() > max_length) {
        this->text = text.substr(0, max_length);
    }
}

void TextInput_t::set_max_length(int max_length) {
    this->max_length = max_length;
    test_truncate();
}

void TextInput_t::set_text(const std::string& text) {
    this->text = text;
    //set_cursor_position(text.length());
    set_cursor_position(0);
    test_truncate();
}

std::string TextInput_t::get_text() const {
    return text;
}

void TextInput_t::set_text_renderer(TextRenderer* text_renderer) {
    this->text_renderer = text_renderer;    
    font_line_height = text_renderer->get_font_line_height();
    set_cursor_position(0);
//    calc_cursor_pixel_position();
}

void TextInput_t::calc_cursor_pixel_position() {
    if (text_renderer == nullptr) {
        return;
    }
    int width, height;
    if (cursor_position > 0) {
        TTF_GetStringSize(text_renderer->font, text.c_str(), cursor_position, &width, &height);
        font_line_height = height;
        cursor_pixel_pos = width;
    } else {
        cursor_pixel_pos = 0;
    } 
}

void TextInput_t::set_cursor_position(int position) {
    if (position < 0) {
        position = 0;
    }
    if (position > text.length()) {
        position = text.length();
    }
    cursor_position = position;
    calc_cursor_pixel_position();
}

void TextInput_t::set_edit_active(bool active) {
    edit_active = active;
    if (active) {
        cursor_state = 0;
    }
}

void TextInput_t::set_cursor_position_by_pixel(int pixel_pos) {
    if ((pixel_pos < tp.x) || (pixel_pos > tp.x + tp.w)) {
        return;
    }
    /* if ((pixel_pos < y) || (pixel_pos > y + h)) {
        return;
    } */
    int rel_pixel = pixel_pos - tp.x;

    int current_pos = 0;
    int accumulated_width = 0;
    int char_width = 0;
    int char_height = 0;

    // Iterate through each character to find the closest position
    for (size_t i = 0; i < text.length(); i++) {
        char_width = text_renderer->char_width(text[i]);
        if (accumulated_width + (char_width / 2) > rel_pixel) {
            break;
        }
        accumulated_width += char_width;
        current_pos++;
    }
    
    set_cursor_position(current_pos);
}

void TextInput_t::set_enter_handler(EventHandler handler) {
    enter_handler = handler;
}

void TextInput_t::render() {
    int text_line = 0;
    if (text_renderer == nullptr) {
        return;
    }
    cursor_state++;
    if (cursor_state > 60) {
        cursor_state = 0;
    }
    
    // take padding, border etc into account
    int eff_x = tp.x + style.padding + style.border_width;

    // draw the background
    int r,g,b,a;
    r = (style.background_color & 0xFF000000) >> 24;
    g = (style.background_color & 0x00FF0000) >> 16;
    b = (style.background_color & 0x0000FF00) >> 8;
    a = (style.background_color & 0x000000FF);
    if (!edit_active) {
        r = r * 0.7;
        g = g * 0.7;
        b = b * 0.7;
        //a = a * 0.4;
    }
    SDL_SetRenderDrawColor(ctx->renderer, r, g, b, a);
    SDL_FRect eb = {tp.x, tp.y, tp.w, tp.h};
    SDL_RenderFillRect(ctx->renderer, &eb);

    // first, render what is in the text input area.    
    text_renderer->render(text, eff_x, tp.y + style.padding);

    // now, render the text input cursor.
    if (cursor_state < 30 && edit_active) {
        int cursor_x = eff_x + cursor_pixel_pos;
        SDL_SetRenderDrawColor(ctx->renderer, 255, 255, 255, 255);
        SDL_RenderLine(ctx->renderer, cursor_x, tp.y, cursor_x, tp.y + font_line_height);
        SDL_SetRenderDrawColor(ctx->renderer, 0, 0, 0, 255);
    }
}

void TextInput_t::clear_edit() {
    text.clear();
    set_cursor_position(0);
}

bool TextInput_t::handle_mouse_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            int mouse_x = event.button.x;
            int mouse_y = event.button.y;
            if (mouse_x >= tp.x && mouse_x <= tp.x + tp.w && mouse_y >= tp.y && mouse_y <= tp.y + tp.h) {
                set_edit_active(true);
                set_cursor_position_by_pixel(mouse_x);
                return true; // if this was directed to us and we're now editing, claim the event
            } else {
                set_edit_active(false);
                return false;
            }
        }
    }
        if (edit_active) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_LEFT) {
                cursor_position--;
                if (cursor_position < 0) {
                    cursor_position = 0;
                }
                set_cursor_position(cursor_position);
                return true;
            }
            if (event.key.key == SDLK_RIGHT) {
                cursor_position++;
                if (cursor_position > text.length()) {
                    cursor_position = text.length();
                }
                set_cursor_position(cursor_position);
                return true;
            }
            if (event.key.key == SDLK_BACKSPACE) {
                if (cursor_position > 0) {
                    text.erase(cursor_position - 1, 1);
                    cursor_position--;
                    set_cursor_position(cursor_position);
                }
                return true;
            }
            if (event.key.key == SDLK_DELETE) {
                if (cursor_position < text.length()) {
                    text.erase(cursor_position, 1);
                    set_cursor_position(cursor_position);
                }   
                return true;
            }
            if (event.key.key == SDLK_RETURN) {
                if (enter_handler) {
                    enter_handler(event);
                    clear_edit();
                }
                return true;
            }
            // assume printable keyboard character
            SDL_Keycode mapped = SDL_GetKeyFromScancode(event.key.scancode, event.key.mod, false);
            if (mapped >= 32 && mapped <= 126) {
                text.insert(cursor_position, 1, (char)mapped);
                cursor_position++;
                set_cursor_position(cursor_position);
                return true;
            }
        }
    }
    return false;
}
