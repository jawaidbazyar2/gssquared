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
// Aux/language-card and $C1-$CF are separate suites. See Docs/IIeMMURefactor.md.

#include "computer.hpp"
#include "debug.hpp"
#include "devices/iiememory/iiememory.hpp"
#include "mmus/mmu_iie.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

uint64_t debug_level = 0;

namespace {

constexpr uint32_t kRamBytes = 128 * 1024;
constexpr uint32_t kRomBytes = 16 * 1024;

enum class Kind { Ram, Rom, None };

struct Side {
    Kind kind = Kind::None;
    uint32_t off = 0;
    bool operator==(const Side &o) const {
        return kind == o.kind && (kind == Kind::None || off == o.off);
    }
};

struct Axes {
    int store = 0;
    int ramrd = 0;
    int ramwrt = 0;
    int altzp = 0;
    int page2 = 0;
    int hires = 0;
    int intcx = 0;
    int slotc3 = 0;
    int bank = 2;
    int read = 0;
    int write = 1;
    int slot = -1;
};

struct Fixture {
    std::unique_ptr<uint8_t[]> rom;
    computer_t *computer = nullptr;
    MMU_IIe *mmu = nullptr;

    Fixture() : rom(std::make_unique<uint8_t[]>(kRomBytes)) {
        computer = new computer_t(computer_bare_t{});
        mmu = new MMU_IIe(256, static_cast<int>(kRamBytes), rom.get());
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
};

enum class MapWindow { RamLc, Cxxx };

struct Golden {
    std::vector<std::string> cross;
    std::vector<std::string> reset;
    std::vector<std::string> video;
    std::vector<std::string> cxx_ignored;
    std::vector<std::string> cxx_intcx_on;
    std::vector<std::string> cxx_slot;
    std::vector<std::string> cxx_c8;
    std::vector<std::string> cxx_reset;
    std::vector<std::string> cxx_aux_ignored;
};

[[noreturn]] void die(const char *msg) {
    std::fprintf(stderr, "%s\n", msg);
    std::exit(1);
}

Side classify(uint8_t *p, uint8_t *main, uint8_t *rom, int page, const char *which) {
    if (p == nullptr) {
        return {};
    }
    if (p >= main && p < main + kRamBytes) {
        return {Kind::Ram, static_cast<uint32_t>(p - main)};
    }
    if (p >= rom && p < rom + kRomBytes) {
        return {Kind::Rom, static_cast<uint32_t>(p - rom)};
    }
    std::fprintf(stderr, "page %02X %s pointer %p is outside RAM and ROM\n", page, which, static_cast<void *>(p));
    std::exit(1);
}

bool continues(const Side &prev, const Side &next) {
    if (prev.kind != next.kind) {
        return false;
    }
    if (prev.kind == Kind::None) {
        return true;
    }
    return next.off == prev.off + 0x100;
}

std::string fmt_side(const Side &s) {
    char buf[32];
    if (s.kind == Kind::None) {
        return "NONE";
    }
    if (s.kind == Kind::Ram) {
        std::snprintf(buf, sizeof buf, "RAM+0x%05X", s.off);
    } else {
        std::snprintf(buf, sizeof buf, "ROM+0x%04X", s.off);
    }
    return buf;
}

std::string format_ram_prefix(const Axes &a) {
    char buf[160];
    std::snprintf(buf, sizeof buf,
                  "80ST=%d RAMRD=%d RAMWRT=%d ALTZP=%d PAGE2=%d HIRES=%d "
                  "LCBANK=%d LCREAD=%d LCWRITE=%d",
                  a.store, a.ramrd, a.ramwrt, a.altzp, a.page2, a.hires,
                  a.bank, a.read, a.write);
    return buf;
}

std::string format_cxx_prefix(const char *c8, int intcx, int slotc3, unsigned slot) {
    char buf[80];
    std::snprintf(buf, sizeof buf, "C8=%s INTCX=%d SLOTC3=%d SLOT=%02X", c8, intcx, slotc3, slot);
    return buf;
}

std::string format_spans(Fixture &f, MapWindow window) {
    uint8_t *main = f.main();
    uint8_t *rom = f.rom.get();

    struct Pg {
        int page;
        Side r;
        Side w;
    };
    std::vector<Pg> rows;
    rows.reserve(255);

    auto push_range = [&](int lo, int hi) {
        for (int page = lo; page <= hi; page++) {
            page_table_entry_t entry{};
            f.mmu->get_page_table_entry(static_cast<page_t>(page), &entry);
            rows.push_back({page,
                            classify(entry.read_p, main, rom, page, "read"),
                            classify(entry.write_p, main, rom, page, "write")});
        }
    };
    if (window == MapWindow::RamLc) {
        push_range(0x00, 0xBF);
        push_range(0xD0, 0xFF);
    } else {
        push_range(0xC1, 0xCF);
    }

    std::string out;
    for (size_t i = 0; i < rows.size();) {
        size_t j = i;
        while (j + 1 < rows.size()
               && rows[j + 1].page == rows[j].page + 1
               && continues(rows[j].r, rows[j + 1].r)
               && continues(rows[j].w, rows[j + 1].w)) {
            j++;
        }
        char head[16];
        std::snprintf(head, sizeof head, "%02X-%02X", rows[i].page, rows[j].page);
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += head;
        out += " R ";
        out += fmt_side(rows[i].r);
        out += " W ";
        out += fmt_side(rows[i].w);
        i = j + 1;
    }
    return out;
}

std::string format_ram_line(const Axes &a, Fixture &f) {
    return format_ram_prefix(a) + " | " + format_spans(f, MapWindow::RamLc);
}

std::string format_cxx_line(const std::string &prefix, Fixture &f) {
    return prefix + " | " + format_spans(f, MapWindow::Cxxx);
}

void set_write(MMU *mmu, int on, uint32_t off_addr, uint32_t on_addr) {
    mmu->write(on ? on_addr : off_addr, 0);
}

void set_read(MMU *mmu, int on, uint32_t off_addr, uint32_t on_addr) {
    mmu->read(on ? on_addr : off_addr);
}

void drive_switches(MMU *mmu, const Axes &a) {
    set_write(mmu, a.store, 0xC000, 0xC001);
    set_write(mmu, a.ramrd, 0xC002, 0xC003);
    set_write(mmu, a.ramwrt, 0xC004, 0xC005);
    set_write(mmu, a.altzp, 0xC008, 0xC009);
    set_read(mmu, a.page2, 0xC054, 0xC055);
    set_read(mmu, a.hires, 0xC056, 0xC057);
    set_write(mmu, a.intcx, 0xC006, 0xC007);
    set_write(mmu, a.slotc3, 0xC00A, 0xC00B);
}

void prime_cxx(Fixture &f, int intcx, int slotc3, uint8_t slot) {
    Axes a;
    a.intcx = intcx;
    a.slotc3 = slotc3;
    drive_switches(f.mmu, a);
    f.mmu->set_slot_register(slot);
}

struct LcPair {
    int bank;
    int read;
    uint32_t even_addr;
    uint32_t odd_addr;
};

// Even access forces bank, read-enable, and write-protect.
// One matching odd access leaves the map alone. The second enables writes.
const LcPair kLcPairs[] = {
    {2, 0, 0xC082, 0xC081},
    {2, 1, 0xC080, 0xC083},
    {1, 0, 0xC08A, 0xC089},
    {1, 1, 0xC088, 0xC08B},
};

bool parse_side(const std::string &tok, Side &s) {
    if (tok == "NONE") {
        s = {};
        return true;
    }
    unsigned off = 0;
    if (std::sscanf(tok.c_str(), "RAM+0x%x", &off) == 1) {
        s = {Kind::Ram, off};
        return true;
    }
    if (std::sscanf(tok.c_str(), "ROM+0x%x", &off) == 1) {
        s = {Kind::Rom, off};
        return true;
    }
    return false;
}

bool parse_token(const char *&p, Side &s) {
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    const char *start = p;
    while (*p != '\0' && *p != ' ') {
        p++;
    }
    return parse_side(std::string(start, p), s);
}

bool parse_span(const char *&p, int &lo, int &hi, Side &r, Side &w) {
    while (*p == ' ') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    unsigned a = 0;
    unsigned b = 0;
    int n = 0;
    if (std::sscanf(p, "%x-%x R%n", &a, &b, &n) != 2) {
        return false;
    }
    p += n;
    if (!parse_token(p, r)) {
        return false;
    }
    while (*p == ' ') {
        p++;
    }
    if (*p != 'W') {
        return false;
    }
    p++;
    if (!parse_token(p, w)) {
        return false;
    }
    lo = static_cast<int>(a);
    hi = static_cast<int>(b);
    return true;
}

std::string side_at(const Side &start, int delta_pages) {
    Side s = start;
    if (s.kind != Kind::None) {
        s.off = start.off + static_cast<uint32_t>(delta_pages) * 0x100;
    }
    return fmt_side(s);
}

// Expand a snapshot line to per-page "R .. W .." text. Empty on parse failure.
bool expand_pages(const std::string &spans, std::vector<std::pair<int, std::string>> &pages) {
    const char *p = spans.c_str();
    int count = 0;
    while (*p != '\0') {
        int lo = 0;
        int hi = 0;
        Side r;
        Side w;
        if (!parse_span(p, lo, hi, r, w)) {
            return false;
        }
        if (hi < lo) {
            return false;
        }
        for (int page = lo; page <= hi; page++) {
            int delta = page - lo;
            pages.push_back({page, "R " + side_at(r, delta) + " W " + side_at(w, delta)});
            count++;
        }
    }
    return count > 0;
}

void explain_mismatch(const std::string &got, const std::string &expected) {
    const auto gpipe = got.find(" | ");
    const auto epipe = expected.find(" | ");
    if (gpipe == std::string::npos || epipe == std::string::npos) {
        std::fprintf(stderr, "got:      %s\nexpected: %s\n", got.c_str(), expected.c_str());
        return;
    }
    const std::string gpre = got.substr(0, gpipe);
    const std::string epre = expected.substr(0, epipe);
    std::fprintf(stderr, "%s\n", gpre.c_str());
    if (gpre != epre) {
        std::fprintf(stderr, "expected switches %s\n", epre.c_str());
    }

    std::vector<std::pair<int, std::string>> gpages;
    std::vector<std::pair<int, std::string>> epages;
    if (!expand_pages(got.substr(gpipe + 3), gpages) || !expand_pages(expected.substr(epipe + 3), epages)) {
        std::fprintf(stderr, "got:      %s\nexpected: %s\n", got.c_str(), expected.c_str());
        return;
    }

    size_t gi = 0;
    size_t ei = 0;
    auto next_page = [](int page) {
        if (page < 0xBF) {
            return page + 1;
        }
        if (page == 0xBF) {
            return 0xC1;
        }
        return page + 1;
    };
    for (int page = 0x00; page <= 0xFF; page = next_page(page)) {
        if (page == 0xC0) {
            continue;
        }
        const bool ghas = gi < gpages.size() && gpages[gi].first == page;
        const bool ehas = ei < epages.size() && epages[ei].first == page;
        if (!ghas && !ehas) {
            continue;
        }
        if (ghas && ehas && gpages[gi].second == epages[ei].second) {
            gi++;
            ei++;
            continue;
        }
        std::fprintf(stderr, "page %02X got %s\n", page, ghas ? gpages[gi].second.c_str() : "missing");
        std::fprintf(stderr, "page %02X expected %s\n", page, ehas ? epages[ei].second.c_str() : "missing");
        return;
    }
    std::fprintf(stderr, "map matched\n");
}

void require_equal(const std::string &got, const std::string &expected, const char *why) {
    if (got == expected) {
        return;
    }
    std::fprintf(stderr, "%s\n", why);
    explain_mismatch(got, expected);
    std::exit(1);
}

void require_spans(const std::string &got, const std::string &expected, const char *why) {
    if (got == expected) {
        return;
    }
    std::fprintf(stderr, "%s\n", why);
    explain_mismatch("got | " + got, "expected | " + expected);
    std::exit(1);
}

void drive_language_card(Fixture &f, Axes axes, std::vector<std::string> &out) {
    drive_switches(f.mmu, axes);
    for (const LcPair &lc : kLcPairs) {
        f.mmu->read(lc.even_addr);
        const std::string protect = format_spans(f, MapWindow::RamLc);
        f.mmu->read(lc.odd_addr);
        const std::string once = format_spans(f, MapWindow::RamLc);
        axes.bank = lc.bank;
        axes.read = lc.read;
        axes.write = 0;
        require_equal(format_ram_prefix(axes) + " | " + once,
                      format_ram_prefix(axes) + " | " + protect,
                      "one language-card access changed the map");
        f.mmu->read(lc.odd_addr);
        const std::string enabled = format_spans(f, MapWindow::RamLc);
        out.push_back(format_ram_prefix(axes) + " | " + protect);
        axes.write = 1;
        out.push_back(format_ram_prefix(axes) + " | " + enabled);
    }
}

uint8_t pack_slot(int index) {
    static const uint8_t bits[6] = {0x02, 0x04, 0x10, 0x20, 0x40, 0x80};
    uint8_t reg = 0;
    for (int b = 0; b < 6; b++) {
        if (index & (1 << b)) {
            reg = static_cast<uint8_t>(reg | bits[b]);
        }
    }
    return reg;
}

void build_cross(std::vector<std::string> &out) {
    Fixture f;
    for (int store = 0; store < 2; store++) {
        for (int ramrd = 0; ramrd < 2; ramrd++) {
            for (int ramwrt = 0; ramwrt < 2; ramwrt++) {
                for (int altzp = 0; altzp < 2; altzp++) {
                    for (int page2 = 0; page2 < 2; page2++) {
                        for (int hires = 0; hires < 2; hires++) {
                            Axes a;
                            a.store = store;
                            a.ramrd = ramrd;
                            a.ramwrt = ramwrt;
                            a.altzp = altzp;
                            a.page2 = page2;
                            a.hires = hires;
                            drive_language_card(f, a, out);
                        }
                    }
                }
            }
        }
    }
}

void build_cxx_ignored(std::vector<std::string> &out) {
    Fixture f;
    drive_switches(f.mmu, Axes{});
    std::string baseline;
    for (int intcx = 0; intcx < 2; intcx++) {
        for (int slotc3 = 0; slotc3 < 2; slotc3++) {
            Axes a;
            a.intcx = intcx;
            a.slotc3 = slotc3;
            drive_switches(f.mmu, a);
            const std::string spans = format_spans(f, MapWindow::RamLc);
            if (baseline.empty()) {
                baseline = spans;
            } else {
                require_spans(spans, baseline, "INTCXROM/SLOTC3ROM changed the aux map");
            }
            char tag[32];
            std::snprintf(tag, sizeof tag, "INTCX=%d SLOTC3=%d ", intcx, slotc3);
            out.push_back(std::string(tag) + format_ram_line(Axes{}, f));
        }
    }
}

void build_cxx_slot(std::vector<std::string> &out) {
    Fixture f;
    for (int index = 0; index < 64; index++) {
        const unsigned reg = pack_slot(index);
        for (int slotc3 = 0; slotc3 < 2; slotc3++) {
            prime_cxx(f, 0, slotc3, static_cast<uint8_t>(reg));
            out.push_back(format_cxx_line(format_cxx_prefix("default", 0, slotc3, reg), f));
        }
    }
}

void check_slot3_bit_ignored() {
    Fixture f;
    prime_cxx(f, 0, 0, 0x00);
    const std::string clear = format_spans(f, MapWindow::Cxxx);
    f.mmu->set_slot_register(0x08);
    const std::string set = format_spans(f, MapWindow::Cxxx);
    require_spans(set, clear, "slot register bit 3 changed $C1-$CF");
}

void build_cxx_intcx_on(std::vector<std::string> &out) {
    Fixture f;
    prime_cxx(f, 1, 0, 0x00);
    const std::string baseline = format_spans(f, MapWindow::Cxxx);
    out.push_back(format_cxx_line(format_cxx_prefix("intcx", 1, 0, 0x00), f));

    f.mmu->set_slot_register(0xF6);
    require_spans(format_spans(f, MapWindow::Cxxx), baseline, "slot register changed the INTCXROM map");
    f.mmu->set_slot_register(0x08);
    require_spans(format_spans(f, MapWindow::Cxxx), baseline, "slot register bit 3 changed the INTCXROM map");
    f.mmu->write(0xC00B, 0);
    require_spans(format_spans(f, MapWindow::Cxxx), baseline, "SLOTC3ROM changed the INTCXROM map");
    f.mmu->read(0xC300);
    require_spans(format_spans(f, MapWindow::Cxxx), baseline, "$C3xx changed the INTCXROM map");
    f.mmu->read(0xCFFF);
    require_spans(format_spans(f, MapWindow::Cxxx), baseline, "$CFFF changed the INTCXROM map");
}

void build_cxx_c8(std::vector<std::string> &out) {
    const uint8_t slot = 0xF6;
    std::string baseline;
    {
        Fixture f;
        prime_cxx(f, 0, 0, slot);
        baseline = format_spans(f, MapWindow::Cxxx);
        out.push_back(format_cxx_line(format_cxx_prefix("default", 0, 0, slot), f));
    }
    {
        Fixture f;
        prime_cxx(f, 0, 0, slot);
        f.mmu->read(0xC300);
        out.push_back(format_cxx_line(format_cxx_prefix("c3", 0, 0, slot), f));
    }
    {
        Fixture f;
        prime_cxx(f, 0, 1, slot);
        f.mmu->read(0xC300);
        out.push_back(format_cxx_line(format_cxx_prefix("c3", 0, 1, slot), f));
    }

    static const uint32_t kAccess[] = {0xC100, 0xC200, 0xC400, 0xC500, 0xC600, 0xC700};
    static const char *kTags[] = {"C1", "C2", "C4", "C5", "C6", "C7"};
    for (int i = 0; i < 6; i++) {
        Fixture f;
        prime_cxx(f, 0, 0, slot);
        f.mmu->read(kAccess[i]);
        const std::string spans = format_spans(f, MapWindow::Cxxx);
        require_spans(spans, baseline, "slot access changed $C8-$CF");
        out.push_back(format_cxx_line(format_cxx_prefix(kTags[i], 0, 0, slot), f));
    }
    {
        Fixture f;
        prime_cxx(f, 0, 0, slot);
        f.mmu->read(0xC300);
        f.mmu->read(0xCFFF);
        const std::string spans = format_spans(f, MapWindow::Cxxx);
        require_spans(spans, baseline, "$CFFF did not restore $C8-$CF");
        out.push_back(format_cxx_line(format_cxx_prefix("cfff", 0, 0, slot), f));
    }
    {
        Fixture f;
        prime_cxx(f, 1, 0, slot);
        f.mmu->read(0xC300);
        out.push_back(format_cxx_line(format_cxx_prefix("hidden", 1, 0, slot), f));
        f.mmu->write(0xC006, 0);
        out.push_back(format_cxx_line(format_cxx_prefix("revealed", 0, 0, slot), f));
    }
}

void reset_fixture(Fixture &f) {
    f.mmu->reset(false);
    for (auto &handler : f.computer->reset_handlers) {
        handler(false);
    }
}

void build_reset(std::vector<std::string> &out) {
    Fixture f;
    reset_fixture(f);
    out.push_back(format_ram_line(Axes{}, f));
}

void build_cxx_reset(std::vector<std::string> &out) {
    Fixture f;
    reset_fixture(f);
    const unsigned slot = f.mmu->get_slot_register();
    out.push_back(format_cxx_line(format_cxx_prefix("reset", 0, 0, slot), f));
}

void build_cxx_aux_ignored(std::vector<std::string> &out) {
    Fixture f;
    prime_cxx(f, 0, 0, 0xF6);
    const std::string before = format_spans(f, MapWindow::Cxxx);
    out.push_back(format_cxx_line("AUX=0 " + format_cxx_prefix("default", 0, 0, 0xF6), f));

    Axes a;
    a.store = 1;
    a.ramrd = 1;
    a.ramwrt = 1;
    a.altzp = 1;
    a.page2 = 1;
    a.hires = 1;
    drive_switches(f.mmu, a);
    f.mmu->read(0xC088);
    f.mmu->read(0xC08B);
    f.mmu->read(0xC08B);
    const std::string after = format_spans(f, MapWindow::Cxxx);
    require_spans(after, before, "aux/language-card switches changed $C1-$CF");
    out.push_back(format_cxx_line("AUX=1 " + format_cxx_prefix("default", 0, 0, 0xF6), f));
}

void build_video(std::vector<std::string> &out) {
    Fixture f;
    Axes a;
    drive_switches(f.mmu, a);
    const std::string before = format_ram_line(a, f);
    f.mmu->read(0xC050);
    f.mmu->read(0xC051);
    f.mmu->read(0xC052);
    f.mmu->read(0xC053);
    f.mmu->write(0xC00C, 0);
    f.mmu->write(0xC00D, 0);
    f.mmu->write(0xC00E, 0);
    f.mmu->write(0xC00F, 0);
    const std::string after = format_ram_line(a, f);
    require_equal(after, before, "TEXT/MIXED/80COL/alt charset changed the map");
    out.push_back(before);
    out.push_back(after);
}

void check_counts(const Golden &g) {
    if (g.cross.size() != 512) {
        std::fprintf(stderr, "cross: %zu lines, expected 512\n", g.cross.size());
        std::exit(1);
    }
    if (g.cxx_slot.size() != 128) {
        std::fprintf(stderr, "cxx-slot: %zu lines, expected 128\n", g.cxx_slot.size());
        std::exit(1);
    }
    if (g.cxx_c8.size() != 12) {
        std::fprintf(stderr, "cxx-c8: %zu lines, expected 12\n", g.cxx_c8.size());
        std::exit(1);
    }
    if (g.reset.size() != 1 || g.video.size() != 2 || g.cxx_ignored.size() != 4
        || g.cxx_intcx_on.size() != 1 || g.cxx_reset.size() != 1 || g.cxx_aux_ignored.size() != 2) {
        die("side suite produced the wrong number of lines");
    }
    require_equal(g.video[1], g.video[0], "TEXT/MIXED/80COL/alt charset changed the map");
    const auto reset_bar = g.cxx_reset[0].find(" | ");
    const auto default_bar = g.cxx_c8[0].find(" | ");
    require_spans(g.cxx_reset[0].substr(reset_bar + 3), g.cxx_c8[0].substr(default_bar + 3),
                  "$Cxxx reset does not match the default slot map");
}

Golden build_all() {
    Golden g;
    check_slot3_bit_ignored();
    build_cross(g.cross);
    build_reset(g.reset);
    build_video(g.video);
    build_cxx_ignored(g.cxx_ignored);
    build_cxx_intcx_on(g.cxx_intcx_on);
    build_cxx_slot(g.cxx_slot);
    build_cxx_c8(g.cxx_c8);
    build_cxx_reset(g.cxx_reset);
    build_cxx_aux_ignored(g.cxx_aux_ignored);
    check_counts(g);
    return g;
}

void write_section(FILE *fp, const char *name, const std::vector<std::string> &lines) {
    std::fprintf(fp, "[%s]\n", name);
    for (const std::string &line : lines) {
        std::fprintf(fp, "%s\n", line.c_str());
    }
}

void write_golden(const char *path, const Golden &g) {
    FILE *fp = std::fopen(path, "w");
    if (fp == nullptr) {
        std::fprintf(stderr, "cannot write %s\n", path);
        std::exit(1);
    }
    std::fputs("# iie mmu characterization\n"
               "# [cross] [reset] [video-ignored] [cxx-ignored] record $00-$BF and $D0-$FF.\n"
               "# [cxx-intcx-on] [cxx-slot] [cxx-c8] [cxx-reset] [cxx-aux-ignored] record $C1-$CF.\n"
               "# RAM+offset is pointer - main_ram, 17 bits (0x00000-0x1FFFF). Bit 16 set is auxiliary bank 0.\n"
               "# LCWRITE=1 means RAM writes are enabled (_FF_WRITE_ENABLE == 0). LCREAD=1 means RAM reads are enabled.\n"
               "# LCBANK is 1 or 2. SLOTC3=1 means slot 3 card ROM (f_slotc3rom).\n"
               "# RamWorks is not recorded.\n",
               fp);
    write_section(fp, "cross", g.cross);
    write_section(fp, "reset", g.reset);
    write_section(fp, "video-ignored", g.video);
    write_section(fp, "cxx-ignored", g.cxx_ignored);
    write_section(fp, "cxx-intcx-on", g.cxx_intcx_on);
    write_section(fp, "cxx-slot", g.cxx_slot);
    write_section(fp, "cxx-c8", g.cxx_c8);
    write_section(fp, "cxx-reset", g.cxx_reset);
    write_section(fp, "cxx-aux-ignored", g.cxx_aux_ignored);
    std::fclose(fp);
}

bool take_section(Golden &g, const std::string &name, const std::string &line) {
    if (name == "cross") {
        g.cross.push_back(line);
    } else if (name == "reset") {
        g.reset.push_back(line);
    } else if (name == "video-ignored") {
        g.video.push_back(line);
    } else if (name == "cxx-ignored") {
        g.cxx_ignored.push_back(line);
    } else if (name == "cxx-intcx-on") {
        g.cxx_intcx_on.push_back(line);
    } else if (name == "cxx-slot") {
        g.cxx_slot.push_back(line);
    } else if (name == "cxx-c8") {
        g.cxx_c8.push_back(line);
    } else if (name == "cxx-reset") {
        g.cxx_reset.push_back(line);
    } else if (name == "cxx-aux-ignored") {
        g.cxx_aux_ignored.push_back(line);
    } else {
        return false;
    }
    return true;
}

Golden read_golden(const char *path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "cannot read %s\n", path);
        std::exit(1);
    }
    Golden g;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.front() == '[') {
            if (line.back() != ']') {
                die("malformed section header");
            }
            section = line.substr(1, line.size() - 2);
            continue;
        }
        if (section.empty() || !take_section(g, section, line)) {
            std::fprintf(stderr, "unknown section [%s]\n", section.c_str());
            std::exit(1);
        }
    }
    return g;
}

void compare_section(const char *name, const std::vector<std::string> &got, const std::vector<std::string> &expected) {
    if (got.size() != expected.size()) {
        std::fprintf(stderr, "%s: %zu lines, golden has %zu\n", name, got.size(), expected.size());
    }
    const size_t n = got.size() < expected.size() ? got.size() : expected.size();
    for (size_t i = 0; i < n; i++) {
        if (got[i] != expected[i]) {
            std::fprintf(stderr, "%s line %zu\n", name, i + 1);
            explain_mismatch(got[i], expected[i]);
            std::exit(1);
        }
    }
    if (got.size() != expected.size()) {
        std::exit(1);
    }
}

void compare_golden(const Golden &got, const Golden &expected) {
    compare_section("cross", got.cross, expected.cross);
    compare_section("reset", got.reset, expected.reset);
    compare_section("video-ignored", got.video, expected.video);
    compare_section("cxx-ignored", got.cxx_ignored, expected.cxx_ignored);
    compare_section("cxx-intcx-on", got.cxx_intcx_on, expected.cxx_intcx_on);
    compare_section("cxx-slot", got.cxx_slot, expected.cxx_slot);
    compare_section("cxx-c8", got.cxx_c8, expected.cxx_c8);
    compare_section("cxx-reset", got.cxx_reset, expected.cxx_reset);
    compare_section("cxx-aux-ignored", got.cxx_aux_ignored, expected.cxx_aux_ignored);
}

std::string find_line(const std::vector<std::string> &lines, const char *prefix) {
    const std::string key = std::string(prefix) + " |";
    for (const std::string &line : lines) {
        if (line.compare(0, key.size(), key) == 0) {
            return line;
        }
    }
    return {};
}

void print_sample(const char *label, const std::string &line) {
    std::printf("%s\n%s\n\n", label, line.empty() ? "MISSING" : line.c_str());
}

void print_samples(const Golden &g) {
    print_sample("all-off composed",
                 find_line(g.cross,
                           "80ST=0 RAMRD=0 RAMWRT=0 ALTZP=0 PAGE2=0 HIRES=0 "
                           "LCBANK=2 LCREAD=0 LCWRITE=1"));
    print_sample("80STORE+PAGE2+HIRES",
                 find_line(g.cross,
                           "80ST=1 RAMRD=0 RAMWRT=0 ALTZP=0 PAGE2=1 HIRES=1 "
                           "LCBANK=2 LCREAD=0 LCWRITE=1"));
    print_sample("ALTZP bank 1 RAM read, write protect",
                 find_line(g.cross,
                           "80ST=0 RAMRD=0 RAMWRT=0 ALTZP=1 PAGE2=0 HIRES=0 "
                           "LCBANK=1 LCREAD=1 LCWRITE=0"));
    print_sample("reset", g.reset.empty() ? std::string() : g.reset[0]);
    print_sample("INTCXROM on", g.cxx_intcx_on.empty() ? std::string() : g.cxx_intcx_on[0]);
    print_sample("slot F6, SLOTC3ROM clear",
                 find_line(g.cxx_slot, "C8=default INTCX=0 SLOTC3=0 SLOT=F6"));
    print_sample("$C3xx SLOTC3ROM clear",
                 find_line(g.cxx_c8, "C8=c3 INTCX=0 SLOTC3=0 SLOT=F6"));
    print_sample("$C3xx then INTCXROM off",
                 find_line(g.cxx_c8, "C8=revealed INTCX=0 SLOTC3=0 SLOT=F6"));
}

} // namespace

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
    bool generate = false;
    const char *path = nullptr;
    if (argc == 3 && std::strcmp(argv[1], "--generate") == 0) {
        generate = true;
        path = argv[2];
    } else if (argc == 2) {
        path = argv[1];
    } else {
        std::fprintf(stderr, "usage: iiemmutest [--generate] GOLDEN\n");
        return 2;
    }

    const Golden got = build_all();
    if (generate) {
        write_golden(path, got);
        std::printf("wrote %s\n\n", path);
        print_samples(got);
        return 0;
    }

    const Golden expected = read_golden(path);
    compare_golden(got, expected);
    return 0;
}
