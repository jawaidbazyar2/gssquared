/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#pragma once

#include <functional>
#include <string>

#include "Container.hpp"
#include "SlotButton.hpp"
#include "Style.hpp"
#include "UIContext.hpp"
#include "gs2.hpp"

class SlotManager_t;

using SlotNameResolver = std::function<std::string(int slot)>;

/**
 * Column of SlotButtons (slots 7..0). Names come from a resolver so the
 * panel works with SlotManager_t (live OSD) or a draft config (EditSystem).
 */
class SlotsPanel_t : public Container_t {
protected:
    SlotButton *slot_buttons[NUM_SLOTS] = {};
    SlotNameResolver name_resolver;

public:
    SlotsPanel_t(UIContext *ctx, const Style_t& style, SlotNameResolver resolver);

    void set_name_resolver(SlotNameResolver resolver);
    void refresh_names();
    SlotButton *get_slot_button(int slot) const;
};

/** Resolver that reads device names from a SlotManager_t. */
SlotNameResolver slot_manager_name_resolver(SlotManager_t *slot_manager);
