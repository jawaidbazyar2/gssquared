/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// Characterization of the IIe page map as iiememory publishes it today.

#include "iie_mmu_oracle.hpp"

#include "computer.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "mbus/MessageBus.hpp"
#include "mmus/mmu_iie.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

uint64_t debug_level = 0;

struct Fixture {
    static constexpr const char *kName = "MMU_IIe";

    std::unique_ptr<uint8_t[]> rom;
    computer_t *computer = nullptr;
    MMU_IIe *mmu = nullptr;

    Fixture() : rom(std::make_unique<uint8_t[]>(iie_oracle::kRomBytes)) {
        computer = new computer_t(computer_bare_t{});
        mmu = new MMU_IIe(256, static_cast<int>(iie_oracle::kRamBytes), rom.get());
        computer->set_mmu(mmu);
        init_iiememory(computer, SLOT_NONE);
    }

    ~Fixture() {
        delete mmu;
        delete computer;
    }

    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    uint8_t *main() const { return mmu->get_memory_base(); }

    void reset_machine() {
        mmu->reset(false);
        for (auto &handler : computer->reset_handlers) {
            handler(false);
        }
    }
};

computer_t::computer_t(computer_bare_t) {
    for (int i = 0; i < MODULE_NUM_MODULES; i++) {
        module_store[i] = nullptr;
    }
    mbus = new MessageBus();
}

computer_t::~computer_t() {
    delete mbus;
}

void computer_t::set_module_state(module_id_t module_id, void *state) {
    module_store[module_id] = state;
}

void computer_t::register_reset_handler(ResetHandler handler) {
    reset_handlers.push_back(std::move(handler));
}

void computer_t::register_debug_display_handler(std::string name, uint64_t id, DebugDisplayHandler handler) {
    debug_display_handlers.push_back({std::move(name), id, std::move(handler)});
}

int main(int argc, char **argv) {
    return iie_oracle::run<Fixture>(argc, argv);
}
