#include "mmus/mmu_iigs.hpp"

#include <cstdint>

namespace aux_pred {

// Today's range tests. A separate translation unit so the timed loop cannot
// see the body, hoist the switches, or delete the call.

[[gnu::noinline]] uint32_t oracle_read(MMU_IIgs *mmu, uint32_t address) {
    if (address & 0x1'0000) return 0x0'0000;
    const uint32_t page = (address & 0xFF00) >> 8;
    if (mmu->is_80store() && (page >= 0x04 && page <= 0x07)) {
        if (mmu->is_page2()) return 0x1'0000;
        else return 0x0'0000;
    }
    if (mmu->is_80store() && mmu->is_hires() && (page >= 0x20 && page <= 0x3F)) {
        if (mmu->is_page2()) return 0x1'0000;
        else return 0x0'0000;
    }
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && mmu->is_altzp()) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && mmu->is_ramrd()) return 0x1'0000;
    return 0x0'0000;
}

[[gnu::noinline]] uint32_t oracle_write(MMU_IIgs *mmu, uint32_t address) {
    if (address & 0x1'0000) return 0x0'0000;
    const uint32_t page = (address & 0xFF00) >> 8;
    if (mmu->is_80store() && (page >= 0x04 && page <= 0x07)) {
        if (mmu->is_page2()) return 0x1'0000;
        else return 0x0'0000;
    }
    if (mmu->is_80store() && mmu->is_hires() && (page >= 0x20 && page <= 0x3F)) {
        if (mmu->is_page2()) return 0x1'0000;
        else return 0x0'0000;
    }
    if (((page >= 0x00 && page <= 0x01) || (page >= 0xC0 && page <= 0xFF)) && mmu->is_altzp()) return 0x1'0000;
    if ((page >= 0x02 && page <= 0xBF) && mmu->is_ramwrt()) return 0x1'0000;
    return 0x0'0000;
}

// Production lookup. The inline method is compiled into this wrapper.
// The timed loop lives in another file and cannot see this body.

[[gnu::noinline]] uint32_t lookup_read(MMU_IIgs *mmu, uint32_t address) {
    return mmu->calc_aux_read(address);
}

[[gnu::noinline]] uint32_t lookup_write(MMU_IIgs *mmu, uint32_t address) {
    return mmu->calc_aux_write(address);
}

} // namespace aux_pred
