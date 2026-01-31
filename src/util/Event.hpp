/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "gs2.hpp"

#define EVENT_NONE 0
#define EVENT_MODAL_SHOW 1
#define EVENT_MODAL_CLICK 2
#define EVENT_PLAY_SOUNDEFFECT 3
#define EVENT_REFOCUS 4
#define EVENT_SHOW_MESSAGE 5

/**
 * event_id: a 64-bit integer that uniquely identifies the type of event.
 */

class Event {
    protected:
        uint64_t event_type;
        uint64_t event_ts;
        uint64_t event_key;
        uint64_t event_data;

    public:
        Event(uint64_t event_type, uint64_t event_key, uint64_t event_data);
        Event(uint64_t event_type, uint64_t event_key, const char *event_data);
        virtual ~Event() = default;
        uint64_t getEventType() const { return event_type; }
        uint64_t getEventData() const { return event_data; }
        uint64_t getEventKey() const { return event_key; }
        uint64_t getEventTs() const { return event_ts; }
};

