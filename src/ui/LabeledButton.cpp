#include "LabeledButton.hpp"

LabeledButton::LabeledButton(UIContext *ctx, int assetID, const std::string& button_text, int group) : Button_t(ctx, assetID, {}, group) {
    this->text = button_text;
    //set_text_renderer(ctx->text_render);
}

void LabeledButton::render() {
    Button_t::render();
    if (text.length() > 0) {
        ctx->text_render->set_color(0xFF, 0xFF, 0xFF,opacity);
        ctx->text_render->render(text, tp.x + cp.x + (cp.w / 2), tp.y + cp.y + (cp.h - ctx->text_render->get_font_line_height()), TEXT_ALIGN_CENTER);
    }
}

