#include "mmus/mmu_iigs.hpp"
#include "mmus/mmu_iie2.hpp"
#include "mmus/iigs_memory.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace shadow_pred {
[[gnu::noinline]] bool oracle(MMU_IIgs *mmu, uint32_t address);
[[gnu::noinline]] bool lookup(MMU_IIgs *mmu, uint32_t address);
}

namespace {

constexpr uint32_t kCalls = 10000000;
constexpr int kPasses = 5;

struct Machine {
    const char *name = "";
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

bool make_machine(Machine &machine, const char *name, size_t rom_size) {
    machine.name = name;
    machine.rom.assign(rom_size, 0);
    const size_t rom_bank_ff_offset = rom_size - 65536;
    const size_t fast_ram = iigs_memory::fast_ram_bytes(rom_size);
    machine.megaii = new MMU_IIe2(256, 128 * 1024, machine.rom.data() + rom_bank_ff_offset + 0xC000, false);
    machine.mmu = new MMU_IIgs(256, static_cast<int>(fast_ram), static_cast<uint32_t>(rom_size), machine.rom.data(), machine.megaii);
    if (machine.mmu->is_rom03_machine() != (rom_size >= 256 * 1024)) {
        std::fprintf(stderr, "%s: is_rom03_machine()=%d, expected %d\n",
                     name, machine.mmu->is_rom03_machine() ? 1 : 0, rom_size >= 256 * 1024 ? 1 : 0);
        return false;
    }
    return true;
}

bool compare_machine(Machine &machine) {
    const size_t mask_bytes = machine.mmu->shadow_mask_nbytes();
    std::vector<uint8_t> snapshot(mask_bytes);
    std::memcpy(snapshot.data(), machine.mmu->shadow_mask_bytes(), mask_bytes);

    for (int reg = 0; reg <= 0xFF; reg++) {
        machine.mmu->set_shadow_register(static_cast<uint8_t>(reg));
        for (uint32_t slot = 0; slot < 128; slot++) {
            const uint32_t addresses[2] = {slot << 10, (slot << 10) | 0x20000};
            for (uint32_t address : addresses) {
                const bool oracle = shadow_pred::oracle(machine.mmu, address);
                const bool lookup = shadow_pred::lookup(machine.mmu, address);
                if (oracle != lookup) {
                    std::printf("mismatch %s reg %02X address %06X oracle %d lookup %d\n",
                                machine.name, reg, address, oracle ? 1 : 0, lookup ? 1 : 0);
                    return false;
                }
            }
        }
    }

    if (std::memcmp(snapshot.data(), machine.mmu->shadow_mask_bytes(), mask_bytes) != 0) {
        std::printf("%s: mask changed during set_shadow_register sweep\n", machine.name);
        return false;
    }

    machine.mmu->reset();
    if (std::memcmp(snapshot.data(), machine.mmu->shadow_mask_bytes(), mask_bytes) != 0) {
        std::printf("%s: mask changed across reset\n", machine.name);
        return false;
    }
    if (machine.mmu->shadow_register() != 0x08) {
        std::printf("%s: shadow register after reset is %02X, expected 08\n",
                    machine.name, machine.mmu->shadow_register());
        return false;
    }
    return true;
}

[[gnu::noinline]] uint64_t oracle_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const bool result = shadow_pred::oracle(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

[[gnu::noinline]] uint64_t lookup_pass(MMU_IIgs *mmu) {
    uint64_t sum = 0;
    for (uint32_t i = 0; i < kCalls; i++) {
        const uint32_t address = i & 0x1FFFF;
        const bool result = shadow_pred::lookup(mmu, address);
        sum += static_cast<uint64_t>(result) * (address | 1u);
    }
    return sum;
}

double median_ns(double *ns_per) {
    std::sort(ns_per, ns_per + kPasses);
    return ns_per[kPasses / 2];
}

bool time_one(Machine &machine, uint8_t reg, double *oracle_ns, double *lookup_ns, uint64_t *checksum) {
    machine.mmu->set_shadow_register(reg);

    // Memory clobbers keep every pass live. A visible pure callee gets
    // deleted from all but the last timed iteration.
    auto keep = [](uint64_t &sum) {
        asm volatile("" : "+r"(sum) : : "memory");
    };

    asm volatile("" ::: "memory");
    (void)oracle_pass(machine.mmu);
    uint64_t oracle_sum = 0;
    for (int pass = 0; pass < kPasses; pass++) {
        const auto t0 = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        uint64_t sum = oracle_pass(machine.mmu);
        keep(sum);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        oracle_ns[pass] = ns / static_cast<double>(kCalls);
        oracle_sum = sum;
    }

    asm volatile("" ::: "memory");
    (void)lookup_pass(machine.mmu);
    uint64_t lookup_sum = 0;
    for (int pass = 0; pass < kPasses; pass++) {
        const auto t0 = std::chrono::steady_clock::now();
        asm volatile("" ::: "memory");
        uint64_t sum = lookup_pass(machine.mmu);
        keep(sum);
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        lookup_ns[pass] = ns / static_cast<double>(kCalls);
        lookup_sum = sum;
    }

    if (oracle_sum != lookup_sum) {
        std::printf("checksum mismatch %s reg %02X oracle %llu lookup %llu\n",
                    machine.name, reg,
                    static_cast<unsigned long long>(oracle_sum),
                    static_cast<unsigned long long>(lookup_sum));
        return false;
    }
    *checksum = oracle_sum;
    return true;
}

void print_times(const char *name, uint8_t reg, const char *which, double *ns_per) {
    std::printf("%s reg %02X %s ns/call:", name, reg, which);
    for (int pass = 0; pass < kPasses; pass++) {
        std::printf(" %.3f", ns_per[pass]);
    }
    std::printf("  median %.3f\n", median_ns(ns_per));
}

bool bench_machine(Machine &machine) {
    static const uint8_t kRegs[] = {0x08, 0x00, 0x3F};
    for (uint8_t reg : kRegs) {
        double oracle_ns[kPasses];
        double lookup_ns[kPasses];
        uint64_t checksum = 0;
        if (!time_one(machine, reg, oracle_ns, lookup_ns, &checksum)) {
            return false;
        }
        print_times(machine.name, reg, "oracle", oracle_ns);
        print_times(machine.name, reg, "lookup", lookup_ns);
        std::printf("%s reg %02X checksum %llu\n",
                    machine.name, reg, static_cast<unsigned long long>(checksum));
    }
    return true;
}

} // namespace

int run_shadow_predicate() {
    Machine rom01;
    Machine rom03;
    if (!make_machine(rom01, "ROM01", 128 * 1024)) {
        return 1;
    }
    if (!make_machine(rom03, "ROM03", 256 * 1024)) {
        return 1;
    }

    if (!compare_machine(rom01) || !compare_machine(rom03)) {
        return 1;
    }
    std::printf("compare passed (ROM01 and ROM03)\n");

    if (!bench_machine(rom01) || !bench_machine(rom03)) {
        return 1;
    }
    return 0;
}
