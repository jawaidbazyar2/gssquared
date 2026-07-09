
#include "TextInput.hpp"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"
#include "UIContext.hpp"

TextInput_t::TextInput_t(UIContext *ctx, const std::string& text, const Style_t& style) : Tile_t(ctx, style) {
    set_text(text);
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
    if (position > (int)text.length()) {
        position = (int)text.length();
    }
    cursor_position = position;
    calc_cursor_pixel_position();
}

void TextInput_t::set_edit_active(bool active) {
    edit_active = active;
    if (active) {
        cursor_blink_ms = SDL_GetTicks();
        cursor_visible = true;
    }
}

void TextInput_t::reset_cursor_blink() {
    cursor_blink_ms = SDL_GetTicks();
    cursor_visible = true;
}

void TextInput_t::set_cursor_position_by_pixel(int pixel_pos) {
    if ((pixel_pos < tp.x) || (pixel_pos > tp.x + tp.w)) {
        return;
    }
    int rel_pixel = pixel_pos - tp.x;

    int current_pos = 0;
    int accumulated_width = 0;
    int char_width = 0;

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

uint32_t TextInput_t::dim(uint32_t color, float level) {
    float r = (color & 0xFF000000) >> 24;
    float g = (color & 0x00FF0000) >> 16;
    float b = (color & 0x0000FF00) >> 8;
    float a = (color & 0x000000FF);

    r *= level;
    g *= level;
    b *= level;

    return ((int)r << 24) | ((int)g << 16) | ((int)b << 8) | (int)a;
}

uint32_t TextInput_t::cursor_color() const {
    // Contrast against field background (white cursor was invisible on light fields).
    const uint32_t bg = edit_active ? style.background_color : dim(style.background_color, 0.7f);
    const int r = (bg >> 24) & 0xFF;
    const int g = (bg >> 16) & 0xFF;
    const int b = (bg >> 8) & 0xFF;
    if (r + g + b > 380) {
        return 0x000000FF;
    }
    return 0xFFFFFFFF;
}

void TextInput_t::update() {
    if (!edit_active) return;
    const Uint64 now = SDL_GetTicks();
    const bool visible = ((now - cursor_blink_ms) / (kCursorBlinkPeriodMs / 2)) % 2 == 0;
    cursor_visible = visible;
}

void TextInput_t::render() {
    if (text_renderer == nullptr) {
        return;
    }

    // Advance blink even if the host only calls render() (e.g. debug window).
    update();

    int eff_x = tp.x + style.padding + style.border_width;

    uint32_t bgcolor = edit_active ? style.background_color : dim(style.background_color, 0.7f);

    SDL_FRect eb = {tp.x, tp.y, tp.w, tp.h};
    ctx->fill_rect(eb, bgcolor);
    if (style.border_width > 0) {
        ctx->draw_rect(eb, style.border_color);
    }

    text_renderer->set_color(
        (style.text_color >> 24) & 0xFF,
        (style.text_color >> 16) & 0xFF,
        (style.text_color >> 8) & 0xFF,
        style.text_color & 0xFF);
    text_renderer->render(text, eff_x, tp.y + style.padding);

    if (cursor_visible && edit_active) {
        int cursor_x = eff_x + cursor_pixel_pos;
        int cursor_h = font_line_height > 0 ? font_line_height : (int)tp.h - 2 * style.padding;
        if (cursor_h < 1) cursor_h = (int)tp.h;
        ctx->line(cursor_x, tp.y + style.padding, cursor_x, tp.y + style.padding + cursor_h, cursor_color());
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
                return true;
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
                reset_cursor_blink();
                return true;
            }
            if (event.key.key == SDLK_RIGHT) {
                cursor_position++;
                if (cursor_position > (int)text.length()) {
                    cursor_position = (int)text.length();
                }
                set_cursor_position(cursor_position);
                reset_cursor_blink();
                return true;
            }
            if (event.key.key == SDLK_BACKSPACE) {
                if (cursor_position > 0) {
                    text.erase(cursor_position - 1, 1);
                    cursor_position--;
                    set_cursor_position(cursor_position);
                }
                reset_cursor_blink();
                return true;
            }
            if (event.key.key == SDLK_DELETE) {
                if (cursor_position < (int)text.length()) {
                    text.erase(cursor_position, 1);
                    set_cursor_position(cursor_position);
                }
                reset_cursor_blink();
                return true;
            }
            if (event.key.key == SDLK_RETURN) {
                if (enter_handler) {
                    enter_handler(event);
                    clear_edit();
                }
                return true;
            }
            SDL_Keycode mapped = SDL_GetKeyFromScancode(event.key.scancode, event.key.mod, false);
            if (mapped >= 32 && mapped <= 126) {
                text.insert(cursor_position, 1, (char)mapped);
                cursor_position++;
                set_cursor_position(cursor_position);
                reset_cursor_blink();
                return true;
            }
        }
    }
    return false;
}
