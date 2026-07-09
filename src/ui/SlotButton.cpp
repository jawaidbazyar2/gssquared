#include "Button.hpp"
#include "SlotButton.hpp"

const Style_t SlotButton_Style = {
    .background_color = 0x0084C6FF,
    .border_color = 0xFFFFFFFF,
    .hover_color = 0x606060FF,
    .padding = 4,
    .border_width = 1,
    .text_color = 0xFFFFFFFF,
};

SlotButton::SlotButton(UIContext *ctx, int assetID, int group, int slot_number, const std::string& device_name)
  : Button_t(ctx, "", SlotButton_Style, group) {
    this->slot_number = slot_number;
    this->slot_type = (SlotType_t)slot_number;
    this->slot_string = std::to_string(slot_number);
    this->slot_device_name = device_name;
}

void SlotButton::set_device_name(const std::string& name) {
    slot_device_name = name;
}

void SlotButton::render() {
    
    Button_t::render();
    
    ctx->text_render->set_color(0x00, 0x00, 0x00, opacity);
    ctx->text_render->render(slot_string, tp.x -15, tp.y + cp.y + (cp.h - ctx->text_render->get_font_line_height()), TEXT_ALIGN_LEFT);

    ctx->text_render->set_color(0xFF, 0xFF, 0xFF, opacity);
    if (slot_device_name.length() > 0) {
        ctx->text_render->render(slot_device_name, tp.x + 20, tp.y + cp.y + (cp.h - ctx->text_render->get_font_line_height()), TEXT_ALIGN_LEFT);
    }
}
