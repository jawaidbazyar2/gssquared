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

#include <string>
#include "Container.hpp"
#include "UIContext.hpp"
#include "util/StorageDevice.hpp"
#include <stack>

struct modal_stack;

class ModalContainer_t : public Container_t {
protected:
    std::string msg_text;
    storage_key_t key;
    uint64_t data;
    bool completed = false;
    bool canceled = false;
    modal_stack &stack;

public:
    ModalContainer_t(UIContext *ctx, const char* msg_text, const Style_t& initial_style, modal_stack &stack);
    ModalContainer_t(UIContext *ctx, const char* msg_text, modal_stack &stack);
    
    bool is_completed() const { return completed; }
    bool is_canceled() const { return canceled; }
    void layout() override;
    void render() override;
    void set_key(storage_key_t key);
    storage_key_t get_key() const;
    void set_data(uint64_t data);
    uint64_t get_data() const;
};

struct modal_stack {
    std::stack<ModalContainer_t *> stack;
};