#include "mmus/mmu_iigs.hpp"
#include "mmus/iigs_shadow_flags.hpp"

#include <cstdint>

namespace shadow_pred {

// Today's range tests. A separate translation unit so the timed loop cannot
// see the body, hoist shadow_register(), or delete the call.
[[gnu::noinline]] bool oracle(MMU_IIgs *mmu, uint32_t address) {
    const uint8_t reg_shadow = mmu->shadow_register();
    const bool is_rom03 = mmu->is_rom03_machine();
    uint32_t address_16 = address & 0xFFFF;
    uint32_t address_17 = address & 0x1FFFF;
    const bool odd_bank = (address & 0x10000) != 0;

    if ((address_16 >= 0x0400 && address_16 <= 0x07FF) && !(reg_shadow & SHADOW_INH_TEXT1)) {
        return true;
    }
    if (is_rom03 && ((address_16 >= 0x0800 && address_16 <= 0x0BFF) && !(reg_shadow & SHADOW_INH_TEXT2))) {
        return true;
    }
    // Even banks: the page inhibit alone. Odd banks: the page inhibit and
    // AUXHGR must both be clear. SHR is a separate enable for odd $2000-$9FFF.
    // SHADOW=$0A sets HGR1 and SHR, so 01:2000 is not copied.
    if (!odd_bank) {
        if ((address_16 >= 0x2000 && address_16 <= 0x3FFF) && !(reg_shadow & SHADOW_INH_HGR1)) {
            return true;
        }
        if ((address_16 >= 0x4000 && address_16 <= 0x5FFF) && !(reg_shadow & SHADOW_INH_HGR2)) {
            return true;
        }
    } else {
        if ((address_16 >= 0x2000 && address_16 <= 0x3FFF)
            && !(reg_shadow & SHADOW_INH_HGR1)
            && !(reg_shadow & SHADOW_INH_AUXHGR)) {
            return true;
        }
        if ((address_16 >= 0x4000 && address_16 <= 0x5FFF)
            && !(reg_shadow & SHADOW_INH_HGR2)
            && !(reg_shadow & SHADOW_INH_AUXHGR)) {
            return true;
        }
    }
    if ((address_17 >= 0x12000 && address_17 <= 0x19FFF) && !(reg_shadow & SHADOW_INH_SHR)) {
        return true;
    }
    return false;
}

// Production lookup. The inline method is compiled into this wrapper.
// The timed loop lives in another file and cannot see this body.
[[gnu::noinline]] bool lookup(MMU_IIgs *mmu, uint32_t address) {
    return mmu->shadow_is_enabled(address);
}

} // namespace shadow_pred
