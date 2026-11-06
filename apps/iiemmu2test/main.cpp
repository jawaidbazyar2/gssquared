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

// Same golden sequences as iiemmutest, driven by MMU_IIe2 alone.

#include "../iiemmutest/iie_mmu_oracle.hpp"

#include "mmus/mmu.hpp"
#include "mmus/mmu_iie2.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

uint64_t debug_level = 0;

struct Fixture {
    static constexpr const char *kName = "MMU_IIe2";

    std::unique_ptr<uint8_t[]> rom;
    MMU_IIe2 *mmu = nullptr;

    Fixture() : rom(std::make_unique<uint8_t[]>(iie_oracle::kRomBytes)) {
        mmu = new MMU_IIe2(256, static_cast<int>(iie_oracle::kRamBytes), rom.get());
    }

    ~Fixture() { delete mmu; }

    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;

    uint8_t *main() const { return mmu->get_memory_base(); }

    void reset_machine() { mmu->reset(false); }
};

static void check_setters() {
    using namespace iie_oracle;
    std::string bus;
    {
        Fixture f;
        Axes a;
        a.store = 1;
        a.ramrd = 1;
        a.page2 = 1;
        a.hires = 1;
        a.altzp = 1;
        drive_switches(f.mmu, a);
        bus = format_spans(f, MapWindow::RamLc);
    }
    {
        Fixture f;
        f.mmu->set_80store(true);
        f.mmu->set_ramrd(true);
        f.mmu->set_page2(true);
        f.mmu->set_hires(true);
        f.mmu->set_altzp(true);
        f.mmu->compose_map(false);
        require_spans(format_spans(f, MapWindow::RamLc), bus, "setters diverged from $C0xx");
    }
}

static uint8_t *page_read(MMU_IIe2 *mmu, page_t page) {
    page_table_entry_t entry{};
    mmu->get_page_table_entry(page, &entry);
    return entry.read_p;
}

static void require_ptr(uint8_t *got, uint8_t *expected, const char *why) {
    if (got == expected) {
        return;
    }
    std::fprintf(stderr, "%s\n  got %p expected %p\n", why, static_cast<void *>(got),
                 static_cast<void *>(expected));
    std::exit(1);
}

// $C073 with RAMRD already on must republish. Bank 0 is main_ram+0x10000.
// Bank 1 is aux_extra. ALTZP language-card bank 2 is cpu_aux+$C000.
static void check_ramworks() {
    Fixture f;
    MMU_IIe2 *mmu = f.mmu;
    mmu->install_ramworks();
    mmu->write(0xC003, 0);

    mmu->set_aux_bank(1);
    if (mmu->get_aux_bank() != 1) {
        std::fprintf(stderr, "RamWorks bank 1 did not stick\n");
        std::exit(1);
    }
    const uint32_t page8_off = 0x08u * GS2_PAGE_SIZE;
    uint8_t *aux0_page8 = f.main() + MMU_IIe2::kAuxBankBytes + page8_off;
    uint8_t *bank1_page8 = mmu->aux_extra + page8_off;
    require_ptr(page_read(mmu, 0x08), bank1_page8, "RAMRD page $08 stayed off AUX 1");

    mmu->set_aux_bank(0);
    if (mmu->get_aux_bank() != 0) {
        std::fprintf(stderr, "RamWorks bank 0 did not stick\n");
        std::exit(1);
    }
    require_ptr(page_read(mmu, 0x08), aux0_page8, "RAMRD page $08 did not return to AUX 0");

    mmu->set_aux_bank(1);
    mmu->write(0xC009, 0);
    mmu->read(0xC080);
    require_ptr(page_read(mmu, 0xD0), mmu->cpu_aux_base() + 0xC000,
                "ALTZP language-card bank 2 is not at cpu_aux+$C000");
}

// 10M reads through MMU*, addresses in $0000-$BFFF. One warmup, five timed passes.
static int run_bench() {
    constexpr uint32_t kReads = 10000000;
    constexpr int kPasses = 5;

    Fixture f;
    MMU *mmu = f.mmu;

    auto pass = [&]() {
        uint64_t sum = 0;
        for (uint32_t i = 0; i < kReads; i++) {
            sum += mmu->read(i % 0xC000u);
        }
        return sum;
    };

    (void)pass();

    double ns_per[kPasses];
    uint64_t sum = 0;
    for (int p = 0; p < kPasses; p++) {
        const auto t0 = std::chrono::steady_clock::now();
        sum = pass();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        ns_per[p] = ns / static_cast<double>(kReads);
    }
    std::sort(ns_per, ns_per + kPasses);
    const double median = ns_per[kPasses / 2];

    std::printf("checksum %llu\n", static_cast<unsigned long long>(sum));
    std::printf("ns/read:");
    for (int p = 0; p < kPasses; p++) {
        std::printf(" %.3f", ns_per[p]);
    }
    std::printf("\nmedian %.3f ns/read  %.3f MHz\n", median, 1000.0 / median);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--bench") == 0) {
        return run_bench();
    }
    const int rc = iie_oracle::run<Fixture>(argc, argv);
    if (rc != 0) {
        return rc;
    }
    check_setters();
    check_ramworks();
    return 0;
}
