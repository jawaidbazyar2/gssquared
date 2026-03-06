#pragma once

#include "Tile.hpp"


class TextInput_t : public Tile_t {
public:
    TextInput_t(UIContext *ctx, const std::string& text, const Style_t& style = Style_t());
    void render() override;
    //void on_click(const SDL_Event& event) override;
    void set_text_renderer(TextRenderer* text_renderer);
    void set_text(const std::string& text);
    std::string get_text() const;
    void set_cursor_position(int position);
    void set_max_length(int max_length);
    bool handle_mouse_event(const SDL_Event& event) override;
    void set_edit_active(bool active);
    bool is_edit_active() const { return edit_active; }
    void set_enter_handler(EventHandler handler);
    void clear_edit();

protected:
    void test_truncate();
    void calc_cursor_pixel_position();
    void set_cursor_position_by_pixel(int pixel_pos);

    std::string text;
    int cursor_position = 0;
    int cursor_pixel_pos = 0;
    int cursor_state = 0;
    TextRenderer* text_renderer = nullptr;
    int font_line_height = 0;
    int max_length = 0; // 0 default means no limit
    bool edit_active = false;
    EventHandler enter_handler = nullptr;

    using EventHandler = std::function<bool(const SDL_Event&)>;

};
