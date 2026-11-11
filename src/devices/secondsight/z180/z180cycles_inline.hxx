/*
 *   Instruction-access helpers for Second Sight's Zilog Z8S180 processor.
 *   Z180.cpp includes these definitions after its private member declarations.
 *   Only the CPU execution thread calls them. They turn logical reads/writes
 *   into recorded bus operations and return sampled bytes to opcode handlers.
 *
 *   PHI is the processor clock; half-PHI is one clock edge interval. Ti means
 *   an internal clock with no memory/port transfer. MMU means memory management
 *   unit, translating logical addresses to physical memory. For direct reads,
 *   SSCard supplies a stable pointer with no read side effect; its byte
 *   still drives the emulated data bus. FRC is the free-running divider;
 *   PRTs are programmable reload timers. OMCR is the operation-mode control
 *   register, including the RETI (return from interrupt) bus-sequence choice.
 *   Test execution records completed accesses so it can restart a handler
 *   without repeating device effects.
 *
 *   Source: Zilog UM005004-0918 pp. 19-30 and Tables 51-52, pp. 251-280.
 *   Deferring clocks and bypassing callbacks are implementation choices, valid
 *   only within the next board deadline and with unchanged pins.
 */

// Advance whole PHI and peripheral clocks up to the scheduler deadline (UM0050 pp. 86, 157-164).
inline void Z180::cycle_clock_span(unsigned phi) {
    assert(!advance_left && icount >= int(2 * phi) && half_phi + 2 * phi <= timer_limit);
    icount -= 2 * phi;
    half_phi += 2 * phi;
    advance_dividers(phi);
    if (!(rcr & 0x80))
        refresh_phase = (refresh_phase + 2 * phi) % 160;
    if (rcr & 0x80) {
        // With refresh on, a due request must reach the next bus boundary,
        // so its phase advances now; spans are single cycles (UM0050 p. 86).
        unsigned const period = 20U << (rcr & 3), ticks = 2 * phi;
        if (refresh_phase % period + ticks >= period && !bus_ack)
            refresh_pending = true;
        refresh_phase = (refresh_phase + ticks) % 160;
    }
}

// Encode the UM0050 Tables 51-52 Ti rules once. The fixed rules can be
// tabulated; trap entry and the OMCR-dependent RETI sequence remain dynamic.
// family/op select the opcode table and byte; n counts completed bus accesses.
// Return whole internal clocks before this access, with write distinguishing
// read/modify/write instructions and reti_mode selecting the longer RETI bus form.
static constexpr unsigned z180_prelude_rule(unsigned family, unsigned op, unsigned n, bool write, bool reti_mode) {
    if (family == Z180_TABLE_op) {
        if (op == 0x10 && n == 1)
            return 1; // UM p256: its following boundary is inhibited.
        if ((op == 0x02 || op == 0x12 || (op >= 0x70 && op <= 0x77 && op != 0x76)) && n == 1)
            return 1;
        if ((op == 0x22 || op == 0x32 || op == 0xcd || (op & 0xc7) == 0xc4) && n == 3)
            return 1;
        if ((op == 0x34 || op == 0x35) && n == 2)
            return 1;
        if (((op & 0xcf) == 0xc5 || (op & 0xc7) == 0xc7) && n == 1)
            return 2;
        if ((op & 0xc7) == 0xc0 && n == 1)
            return 1;
        if (op == 0xe3 && n == 3)
            return 1;
        if (op == 0xd3 && n == 2)
            return 1;
    } else if (family == Z180_TABLE_xy) {
        if (op == 0x22 && n == 4)
            return 1;
        if (op == 0xe3 && n == 4)
            return 1;
        if (op == 0xe5 && n == 2)
            return 2;
        if ((op == 0x34 || op == 0x35) && n == 4)
            return 1;
        if (n == 3 && (op == 0x34 || op == 0x35 || (op >= 0x40 && op < 0xc0 && (op & 7) == 6)))
            return 2;
        if (n == 3 && op >= 0x70 && op <= 0x77 && op != 0x76)
            return 3;
    } else if (family == Z180_TABLE_ed) {
        if (op < 0x40 && (op & 7) == 1 && n == 3)
            return 1;
        if (op >= 0x40 && op < 0x80 && (op & 7) == 1 && n == 2)
            return 1;
        if ((op == 0x34 || op == 0x83 || op == 0x8b || op == 0x93 || op == 0x9b) && n == 2)
            return 1;
        if ((op & 0xcf) == 0x43 && n == 4)
            return 1;
        if ((op == 0x67 || op == 0x6f) && n == 3)
            return 4;
        if (op == 0x4d && reti_mode) {
            if (n == 2)
                return 3;
            if (n == 3)
                return 1;
        }
    } else if (family == Z180_TABLE_cb && write && n == 3)
        return 1;
    else if (family == Z180_TABLE_xycb && write && n == 5)
        return 1;
    return 0;
}

// Pack eight bus-position counts into one word per opcode. Each three-bit
// field stores 0..7 internal clocks; state-dependent exceptions are applied
// by cycle_prelude after lookup. The table is shared, immutable host data.
static constexpr auto z180_prelude_table() {
    std::array<std::array<uint32_t, 256>, 5> table{};
    for (unsigned f = 0; f != table.size(); ++f)
        for (unsigned op = 0; op != 256; ++op)
            for (unsigned n = 0; n != 8; ++n)
                table[f][op] |= z180_prelude_rule(f, op, n, true, true) << (3 * n);
    return table;
}
static constexpr auto z180_ti_preludes = z180_prelude_table();

// Look up the shared timing rules, retaining the state-dependent bus cases.
inline unsigned Z180::cycle_prelude(cycle_kind kind) const {
    if (trapping || family >= z180_ti_preludes.size() || bus_number >= 8)
        return cycle_prelude_reference(kind);
    unsigned value = (z180_ti_preludes[family][opcode] >> (3 * bus_number)) & 7;
    if ((family == Z180_TABLE_cb || family == Z180_TABLE_xycb) && kind != cycle_kind::write)
        value = 0;
    if (family == Z180_TABLE_ed && opcode == 0x4d && (omcr & 0x80))
        value = 0;
#ifdef GS2_SS_TESTING
    assert(value == cycle_prelude_reference(kind));
#endif
    return value;
}

// Copy current pins and interrupt priority once for a span guaranteed to have no input changes.
inline void Z180::cycle_sample_stable_inputs() {
    cycle_sample_dma_inputs();    sampled_bus_request = bus_released;
    cycle_sample_interrupts();
    fast_interrupt = sampled_interrupt;
    fast_irq_valid = true;
}

// Read the board-approved byte and drive the bus once. Address is logical;
// the test engine records it for replay; production does not retain it.
[[gnu::always_inline]] inline uint8_t Z180::cycle_certified_read(cycle_kind kind, uint32_t address, uint8_t const *direct) {
    // Inputs stay fixed until the deadline, and the page needs no
    // device callback or external WAIT. Include programmed memory waits,
    // drive the bus once and retain the completed-cycle state.
    if (!fast_irq_valid)
        cycle_sample_stable_inputs();
    uint8_t const data = *direct;
    drive_direct_read(data);
    cycle_clock_span(3 + memory_wait_states());
    cycle_completed_record(kind, address, data, false);
    cycle_finish_access(kind, address, data, 3, false);
    return data;
}

// Perform a complete logical access after prelude Ti clocks when the deadline permits it.
// Inline at instruction call sites so the compiler removes tests for other access
// kinds; the callback and resumable paths retain their ordinary shared bodies.
[[gnu::always_inline]] inline uint8_t Z180::cycle_whole_access(cycle_kind kind, uint32_t address, uint8_t data, unsigned prelude) {
    if (prelude) {
        if (rcr & 0x80) {
            // Each Ti boundary may take a due refresh cycle; the access then
            // uses the fast path only if it still fits and no refresh is due.
            for (unsigned n = 0; n < prelude; ++n)
                cycle_transaction(cycle_kind::internal, 0, 0, family == Z180_TABLE_op && opcode == 0x10);
            ++bus_number;
            if (refresh_pending || icount < int(2 * (3 + memory_wait_states())))
                return cycle_transaction(kind, address, data);
        } else {
            cycle_whole_internal(prelude, family == Z180_TABLE_op && opcode == 0x10);
            ++bus_number;
        }
    } else
        ++bus_number;
    if (kind != cycle_kind::write && direct_reads_enabled) {
        if (auto const page = direct_logical[(address >> 12) & 15])
            return cycle_certified_read(kind, address, page + (address & 4095));
    }
    uint32_t const physical = MMU_REMAP_ADDR(address);
    return cycle_mapped_access(kind, address, physical, data);
}

// Perform a logical CPU access; test execution can replay it. data is used for writes.
[[gnu::always_inline]] inline uint8_t Z180::cycle_access(cycle_kind kind, uint32_t address, uint8_t data) {
    unsigned const pre = cycle_prelude(kind);
    // If the whole instruction fits before the deadline, its memory cycles do too.
    // Otherwise check whether this cycle alone fits.
    bool const admitted = instruction_whole || (!cycle_observer && !cycle.active && cursor == history_count);
    if (admitted && instruction_fast && !trapping && !refresh_pending &&
        icount >= int(2 * (pre + 3 + memory_wait_states()))) {
        if (kind <= cycle_kind::write)
            return cycle_whole_access(kind, address, data, pre);
        if ((kind == cycle_kind::input || kind == cycle_kind::output) && !sampled_bus_request && !(rcr & 0x80))
            return cycle_whole_io(kind, address, data, pre);
    }
    return cycle_access_slow(kind, address, data, pre);
}
