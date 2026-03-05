#include "LabeledButton.hpp"

LabeledButton::LabeledButton(AssetAtlas_t* assetp, int assetID, const std::string& button_text, TextRenderer* tr, int group) : Button_t(assetp, assetID, group) {
    this->text = button_text;
    set_text_renderer(tr);
}

void LabeledButton::render(SDL_Renderer* renderer) {
    Button_t::render(renderer);
    if (text.length() > 0) {
        text_render->set_color(0xFF, 0xFF, 0xFF,opacity);
        text_render->render(text, tp.x + cp.x + (cp.w / 2), tp.y + cp.y + (cp.h - text_render->get_font_line_height()), TEXT_ALIGN_CENTER);
    }
}

