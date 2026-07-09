/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "SlotsPanel.hpp"

#include "devices.hpp"
#include "slots.hpp"

SlotsPanel_t::SlotsPanel_t(UIContext *ctx, const Style_t& style, SlotNameResolver resolver)
    : Container_t(ctx, style), name_resolver(std::move(resolver)) {
    for (int i = 7; i >= 0; i--) {
        std::string name = name_resolver ? name_resolver(i) : std::string();
        SlotButton *slot = new SlotButton(ctx, 0, 0, i, name);
        slot->size(300, 30);
        slot_buttons[i] = slot;
        add(slot);
    }
    // Caller must set_position/size then layout() — layout here would use a zero-sized rect.
}

void SlotsPanel_t::set_name_resolver(SlotNameResolver resolver) {
    name_resolver = std::move(resolver);
}

void SlotsPanel_t::refresh_names() {
    if (!name_resolver) return;
    for (int i = 0; i < NUM_SLOTS; i++) {
        if (slot_buttons[i]) {
            slot_buttons[i]->set_device_name(name_resolver(i));
        }
    }
}

SlotButton *SlotsPanel_t::get_slot_button(int slot) const {
    if (slot < 0 || slot >= NUM_SLOTS) return nullptr;
    return slot_buttons[slot];
}

SlotNameResolver slot_manager_name_resolver(SlotManager_t *slot_manager) {
    return [slot_manager](int slot) -> std::string {
        if (!slot_manager) return {};
        Device_t *device = slot_manager->get_device(static_cast<SlotType_t>(slot));
        return device ? device->name : std::string();
    };
}
