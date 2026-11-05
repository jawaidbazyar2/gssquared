#include "mmus/mmu_iigs.hpp"
#include "mmus/mmu_iie2.hpp"
#include "mmus/iigs_memory.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace aux_pred {
[[gnu::noinline]] uint32_t oracle_read(MMU_IIgs *mmu, uint32_t address);
[[gnu::noinline]] uint32_t oracle_write(MMU_IIgs *mmu, uint32_t address);
[[gnu::noinline]] uint32_t lookup_read(MMU_IIgs *mmu, uint32_t address);
[[gnu::noinline]] uint32_t lookup_write(MMU_IIgs *mmu, uint32_t address);
}

namespace {

constexpr uint32_t kCalls = 10000000;
constexpr int kPasses = 5;

constexpr unsigned k80Store = 1u << 0;
constexpr unsigned kPage2 = 1u << 1;
constexpr unsigned kHires = 1u << 2;
constexpr unsigned kAltzp = 1u << 3;
constexpr unsigned kRamrd = 1u << 4;
constexpr unsigned kRamwrt = 1u << 5;

struct Machine {
    std::vector<uint8_t> rom;
    MMU_IIe2 *megaii = nullptr;
    MMU_IIgs *mmu = nullptr;

    Machine() = default;
    Machine(const Machine &) = delete;
    Machine &operator=(const Machine &) = delete;

    ~Machine() {
        delete mmu;
        delete megaii;
    }
};

bool make_machine(Machine &machine) {
    constexpr size_t rom_size = 128 * 1024;
    machine.rom.assign(rom_size, 0);
    const size_t rom_bank_ff_offset = rom_size - 65536;
    const size_t fast_ram = iigs_memory::fast_ram_bytes(rom_size);
    machine.megaii = new MMU_IIe2(256, 128 * 1024, machine.rom.data() + rom_bank_ff_offset + 0xC000, false);
    machine.mmu = new MMU_IIgs(256, static_cast<int>(fast_ram), static_cast<uint32_t>(rom_size), machine.rom.data(), machine.megaii);
    if (machine.mmu->is_rom03_machine()) {
        std::fprintf(stderr, "ROM01: is_rom03_machine()=1, expected 0\n");
        return false;
    }
    return true;
}

void apply_switches(MMU_IIgs *mmu, unsigned bits) {
    mmu->write_c0xx(bits & k80Store ? 0xC001 : 0xC000, 0);
    mmu->write_c0xx(bits & kRamrd ? 0xC003 : 0xC002, 0);
    mmu->write_c0xx(bits & kRamwrt ? 0xC005 : 0xC004, 0);
    mmu->write_c0xx(bits & kAltzp ? 0xC009 : 0xC008, 0);
    mmu->write_c0xx(bits & kPage2 ? 0xC055 : 0xC054, 0);
    mmu->write_c0xx(bits & kHires ? 0xC057 : 0xC056, 0);
}

bool compare_one(MMU_IIgs *mmu, unsigned bits, const char *dir, uint32_t address,
                 uint32_t (*oracle)(MMU_IIgs *, uint32_t),
                 uint32_t (*lookup)(MMU_IIgs *, uint32_t)) {
    const uint32_t got_oracle = oracle(mmu, address);
    const uint32_t got_lookup = lookup(mmu, address);
    if (got_oracle == got_lookup) return true;
    std::printf("mismatch %s 80STORE %d PAGE2 %d HIRES %d ALTZP %d RAMRD %d RAMWRT %d address %06X oracle %X lookup %X\n",
                dir,
                (bits & k80Store) ? 1 : 0,
                (bits & kPage2) ? 1 : 0,
                (bits & kHires) ? 1 : 0,
                (bits & kAltzp) ? 1 : 0,
                (bits & kRamrd) ? 1 : 0,
                (bits & kRamwrt) ? 1 : 0,
                address, got_oracle, got_lookup);
    return false;
}

bool compare_machine(Machine &machine) {
    uint8_t snapshot[256];
    std::memcpy(snapshot, machine.mmu->aux_region_bytes(), sizeof(snapshot));

    for (unsigned bits = 0; bits < 64; bits++) {
        apply_switches(machine.mmu, bits);
        for (uint32_t page = 0; page < 256; page++) {
            const uint32_t addresses[3] = {page << 8, (page << 8) | 0x20000, (page << 8) | 0x10000};
            for (uint32_t address : addresses) {
                if (!compare_one(machine.mmu, bits, "read", address, aux_pred::oracle_read, aux_pred::lookup_read)) {
                    return false;
                }
                if (!compare_one(machine.mmu, bits, "write", address, aux_pred::oracle_write, aux_pred::lookup_write)) {
                    return false;
                }
            }
        }
    }

    if (std::memcmp(snapshot, machine.mmu->aux_region_bytes(), sizeof(snapshot)) != 0) {
        std::printf("region map changed during switch sweep\n");
        return false;
    }
    return true;
}

[[gnu::noinline]] uint64_t oracle_read_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const uint32_t result = aux_pred::oracle_read(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

[[gnu::noinline]] uint64_t lookup_read_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const uint32_t result = aux_pred::lookup_read(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

[[gnu::noinline]] uint64_t oracle_write_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const uint32_t result = aux_pred::oracle_write(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

[[gnu::noinline]] uint64_t lookup_write_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const uint32_t result = aux_pred::lookup_write(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

double median_ns(double *ns_per) {
    std::sort(ns_per, ns_per + kPasses);
    return ns_per[kPasses / 2];
}

using PassFn = uint64_t (*)(MMU_IIgs *);

bool time_pair(MMU_IIgs *mmu, PassFn oracle_pass, PassFn lookup_pass, double *oracle_ns, double *lookup_ns, uint64_t *checksum) {
    auto keep = [](uint64_t &sum) {
        asm volatile("" : "+r"(sum) : : "memory");
    };

    asm volatile("" ::: "memory");
    (void)oracle_pass(mmu);
    uint64_t oracle_sum = 0;
    for (int pass = 0; pass < kPasses; pass++) {
        const auto t0 = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        uint64_t sum = oracle_pass(mmu);
        keep(sum);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        oracle_ns[pass] = ns / static_cast<double>(kCalls);
        oracle_sum = sum;
    }

    asm volatile("" ::: "memory");
    (void)lookup_pass(mmu);
    uint64_t lookup_sum = 0;
    for (int pass = 0; pass < kPasses; pass++) {
        const auto t0 = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        uint64_t sum = lookup_pass(mmu);
        keep(sum);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        lookup_ns[pass] = ns / static_cast<double>(kCalls);
        lookup_sum = sum;
    }

    if (oracle_sum != lookup_sum) {
        std::printf("checksum mismatch oracle %llu lookup %llu\n",
                    static_cast<unsigned long long>(oracle_sum),
                    static_cast<unsigned long long>(lookup_sum));
        return false;
    }
    *checksum = oracle_sum;
    return true;
}

void print_times(const char *state, const char *dir, const char *which, double *ns_per) {
    std::printf("%s %s %s ns/call:", state, dir, which);
    for (int pass = 0; pass < kPasses; pass++) {
        std::printf(" %.3f", ns_per[pass]);
    }
    std::printf("  median %.3f\n", median_ns(ns_per));
}

bool bench_direction(Machine &machine, const char *state, const char *dir, PassFn oracle_pass, PassFn lookup_pass) {
    double oracle_ns[kPasses];
    double lookup_ns[kPasses];
    uint64_t checksum = 0;
    if (!time_pair(machine.mmu, oracle_pass, lookup_pass, oracle_ns, lookup_ns, &checksum)) {
        std::printf("checksum mismatch %s %s\n", state, dir);
        return false;
    }
    print_times(state, dir, "oracle", oracle_ns);
    print_times(state, dir, "lookup", lookup_ns);
    std::printf("%s %s checksum %llu\n", state, dir, static_cast<unsigned long long>(checksum));
    return true;
}

bool bench_machine(Machine &machine) {
    struct State {
        const char *name;
        unsigned bits;
    };
    static const State kStates[] = {
        {"off", 0},
        {"ram", kRamrd | kRamwrt | kAltzp},
        {"store", k80Store | kHires | kRamrd},
    };

    for (const State &state : kStates) {
        apply_switches(machine.mmu, state.bits);
        if (!bench_direction(machine, state.name, "read", oracle_read_pass, lookup_read_pass)) return false;
        if (!bench_direction(machine, state.name, "write", oracle_write_pass, lookup_write_pass)) return false;
    }
    return true;
}

} // namespace

int run_aux_predicate() {
    Machine rom01;
    if (!make_machine(rom01)) {
        return 1;
    }

    if (!compare_machine(rom01)) {
        return 1;
    }
    std::printf("compare passed\n");

    if (!bench_machine(rom01)) {
        return 1;
    }
    return 0;
}
