#include "slots.hpp"
#include "Button.hpp"
#include "SlotButton.hpp"
#include "devices.hpp"

const Style_t SlotButton_Style = {
    .background_color = 0x0084C6FF,
    .border_color = 0xFFFFFFFF,
    .border_width = 1,
    .padding = 4,
    .hover_color = 0x606060FF,
    .text_color = 0xFFFFFFFF,
};

SlotButton::SlotButton(AssetAtlas_t* assetp, int assetID, TextRenderer* tr, int group, int slot_number, SlotManager_t *slot_manager) 
  : Button_t("", tr, SlotButton_Style, group) {
    this->slot_number = slot_number;
    this->slot_manager = slot_manager;
    this->device = slot_manager->get_device((SlotType_t)slot_number);
    this->slot_string = std::to_string(slot_number);
    this->slot_device_name = device->name;
}


void SlotButton::render(SDL_Renderer* renderer) {
    
    Button_t::render(renderer);
    
    text_render->set_color(0x00, 0x00, 0x00, opacity);
    text_render->render(slot_string, tp.x -15, tp.y + cp.y + (cp.h - text_render->get_font_line_height()), TEXT_ALIGN_LEFT);

    text_render->set_color(0xFF, 0xFF, 0xFF, opacity);
    if (slot_device_name.length() > 0) {
        text_render->render(slot_device_name, tp.x + 20, tp.y + cp.y + (cp.h - text_render->get_font_line_height()), TEXT_ALIGN_LEFT);
    }
}