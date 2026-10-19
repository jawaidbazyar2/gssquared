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

#include <cstdint>
#include <cstdio>

#include "debug.hpp"
#include "util/DebugFormatter.hpp"
#include "util/InterruptController.hpp"
#include "regs.hpp"

#define MB_6522_1 0x00
#define MB_6522_2 0x80


class N6522 {
private:
    union ifr_t {
        uint8_t value;
        struct {
            uint8_t ca2 : 1;
            uint8_t ca1 : 1;
            uint8_t shift_register : 1;
            uint8_t cb2 : 1;
            uint8_t cb1 : 1;
            uint8_t timer2 : 1;
            uint8_t timer1 : 1;
            uint8_t irq : 1;
        } bits;
    };

    union ier_t {
        uint8_t value;
        struct {
            uint8_t ca2 : 1;
            uint8_t ca1 : 1;
            uint8_t shift_register : 1;
            uint8_t cb2 : 1;
            uint8_t cb1 : 1;
            uint8_t timer2 : 1;
            uint8_t timer1 : 1;
        } bits;
    };

    uint8_t ora; /* 0x00 */
    uint8_t ira; /* 0x00 */
    uint8_t orb; /* 0x01 */
    uint8_t irb; /* 0x01 */
    uint8_t ddra; /* 0x02 */
    uint8_t ddrb; /* 0x03 */

    uint8_t sr; /* 0x0A */
    uint8_t acr; /* 0x0B */
    uint8_t pcr; /* 0x0C */
    ifr_t ifr; /* 0x0D */
    ier_t ier; /* 0x0E */

    uint16_t t1_latch;
    uint16_t t1_counter;  
    uint16_t t2_latch;
    uint16_t t2_counter;
    uint16_t t1_oneshot_pending = 0;
    uint16_t t2_oneshot_pending = 0;

    bool t1_rollover = false;
    bool t2_rollover = false;

    // On TxC_H write, the real 6522 performs a pure load on that cycle with
    // no decrement; counting resumes on the following cycle. Our cycle_handler
    // runs AFTER the register write within the same wall cycle, so we mark the
    // first incr_cycle() following the write to skip decrementing Tx.
    bool t1_skip_next_decrement = false;
    bool t2_skip_next_decrement = false;

    //uint64_t system_cycles;
    uint64_t t1_next_event;
    uint64_t t2_next_event;

    uint64_t t1_instanceID;
    uint64_t t2_instanceID;

    const char *chip_id;
    NClock *clock;
    InterruptController *irq_control;
    uint8_t slot;
    uint8_t chip;

    const char *reg_names[16] = {
        "ORB/IRB", "ORA/IRA", "DDRB", "DDRA",
        "T1C_L", "T1C_H","T1L_L", "T1L_H",
        "T2L_L", "T2C_H", "SR", "ACR",
        "PCR", "IFR", "IER", "ORANH"
    };

public:
    N6522(const char *chip_id, NClock *clock, InterruptController *irq_controller, uint8_t slot = 0, uint8_t chip = 0) : 
        chip_id(chip_id), clock(clock), irq_control(irq_controller), slot(slot), chip(chip) {
        // Real 6522 power-on state for T1L/T1C/T2L/T2C is undefined garbage
        // (per AppleWin issue #652: "at power-on, all the 6522 regs have
        // random values"). Zero-initialising these traps the timers at
        // $0000 <-> $FFFF in one-shot mode, which breaks games that sample
        // T1C_L for mb detection without first writing T1C_H (e.g. Skyfox's
        // LDA/CMP/SBC at $C404). Use a deterministic pseudo-random mix of
        // slot+chip so behaviour is repeatable across runs while still
        // presenting a non-degenerate latch/counter to unarmed reads.
        uint32_t seed = 0xDEADBEEFu
                      ^ (uint32_t(slot) * 0x9E3779B1u)
                      ^ (uint32_t(chip) * 0x6A09E667u);
        auto mix = [&seed]() -> uint16_t {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            // Force non-zero to avoid accidentally reproducing the degenerate
            // T1L=$0000 case unless the guest explicitly writes it.
            uint16_t v = static_cast<uint16_t>(seed & 0xFFFF);
            return v ? v : uint16_t(0xA5A5);
        };
        t1_latch   = mix();
        t1_counter = mix();
        t2_latch   = mix();
        t2_counter = mix();
        ddra = 0x00;
        ddrb = 0x00;
        ora = 0x00;
        orb = 0x00;
        // Port A/B inputs default to the pulled-up state ($FF) seen on a
        // real Mockingboard when no external device is driving the pins.
        // mb-audit Test6522AfterReset subTests #8/#A verify IRB/IRA==$FF.
        ira = 0xFF;
        irb = 0xFF;
        ifr.value = 0;
        ier.value = 0;
        acr = 0;
        pcr = 0;
        sr = 0;
        //system_cycles = 0;
        t1_next_event = 0;
        t2_next_event = 0;
        t1_rollover = 0;
        t2_rollover = 0;
        t1_skip_next_decrement = false;
        t2_skip_next_decrement = false;

        bool t1_armed = false;
        bool t2_armed = false;

        t1_instanceID = 0x10000000 | (slot << 8) | chip;
        t2_instanceID = 0x10010000 | (slot << 8) | chip;
    };
    ~N6522() {
        irq_control->deassert_irq((device_irq_id)chip);
    };

    uint64_t get_clock_cycles() { return clock->get_vid_cycles(); }

    // Latch a byte onto the input register of Port A. Used by the
    // Mockingboard bridge when the paired AY drives the data bus on a read.
    inline void set_ira(uint8_t value) { ira = value; }

    // Accessors needed by the Mockingboard bridge to derive the AY bus
    // state (post-DDR) from this generic 6522 without leaking write access.
    inline uint8_t get_ora()  const { return ora;  }
    inline uint8_t get_orb()  const { return orb;  }
    inline uint8_t get_ddra() const { return ddra; }
    inline uint8_t get_ddrb() const { return ddrb; }

    // Present the falling edge generated by the SSI-263 A/!R output when a
    // phoneme completes.  CA1 uses PCR bit 0 to select its active edge; the
    // Mockingboard speech circuit presents completion as a falling edge, so a
    // rising-edge configuration must not set IFR.CA1.  The flag latches even
    // when IER.CA1 is disabled; update_interrupt() applies the IER gate only to
    // the outward IRQ line, as a real 6522 does.
    inline void signal_ca1_falling_edge() {
        if ((pcr & 0x01) == 0) {
            ifr.bits.ca1 = 1;
            update_interrupt();
        }
    }

    void update_interrupt() {
        // for each chip, calculate the IFR bit 7.

        bool irq = ((ifr.value & ier.value & 0x7F) > 0) ? true : false;
        // set bit 7 of IFR to the result.
        
        ifr.bits.irq = irq;
        
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("[%s]: chip %d assert irq: %d\n", chip_id, chip, (int)irq);
        irq_control->set_irq((device_irq_id)chip, irq);
        //irq_callback(irq_to_slot);
    }

    void incr_cycle() {

        // T1 Logic
        if (t1_skip_next_decrement) {
            // Consume the load-cycle exemption: no rollover processing or
            // decrement on the cycle that immediately follows a T1C_H write.
            t1_skip_next_decrement = false;
        } else {
            // Real-6522 underflow has two observable effects across two
            // consecutive cycles:
            //   Cycle A (counter enters as 0): IRQ asserts, counter goes to
            //          $FFFF. This is the cycle the CPU sees the interrupt.
            //   Cycle B (counter is $FFFF, rollover pending): continuous
            //          mode reloads the counter from the latch; one-shot
            //          mode simply continues decrementing ($FFFF -> $FFFE).
            // Previously both effects happened on Cycle B, delaying the
            // IRQ by one cycle. That delay was masked by an older
            // "decrement on load cycle" bug; with the load-cycle skip in
            // place it must be corrected here. Preserving the two-cycle
            // underflow keeps the canonical "T1L=$0000 alternates between
            // $00 and $FF" behavior (mb-audit T6522_B).
            if (t1_counter == 0) {
                if (acr & 0x40) { // continuous mode
                    ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
                    update_interrupt();
                } else {         // one-shot mode
                    if (t1_oneshot_pending) {
                        ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
                        update_interrupt();
                    }
                    t1_oneshot_pending = 0;
                }
                t1_rollover = 1;
                t1_counter--; // 0x0000 -> 0xFFFF
            } else if (t1_rollover && (t1_counter == 0xFFFF)) {
                // Real 6522 reloads T1C from T1L on every underflow in both
                // continuous and one-shot modes; only the *interrupt* is
                // one-shot (gated above by t1_oneshot_pending). mb-audit
                // T6522_9 subtest #1 verifies the reload occurs in one-shot.
                t1_counter = t1_latch;
                t1_rollover = 0;
            } else {
                t1_counter--;
            }
        }

        // T2 Logic
        if (t2_skip_next_decrement) {
            t2_skip_next_decrement = false;
        } else {
            // Same single-cycle-earlier IRQ correction as T1: fire on the
            // cycle the counter enters as 0 (i.e. the 0 -> FFFF transition),
            // not on the following FFFF cycle. T2 is one-shot only, so the
            // counter simply continues decrementing past FFFF.
            if (t2_counter == 0) {
                if (t2_oneshot_pending) {
                    ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
                    update_interrupt();
                }
                t2_oneshot_pending = 0;
                t2_rollover = 1;
                t2_counter--; // 0x0000 -> 0xFFFF
            } else if (t2_rollover && (t2_counter == 0xFFFF)) {
                t2_counter--;  // 0xFFFF -> 0xFFFE
                t2_rollover = 0;
            } else {
                t2_counter--;
            }
        }
    }

#if 0
    void t2_timer_callback(uint64_t instanceID, void *user_data) {

        if (DEBUG(DEBUG_MOCKINGBOARD)) std::cout << "MB 6522 Timer callback " << chip_id << std::endl;

        // TODO: "after timing out, the counter will continue to decrement."
        // so do NOT reset the counter to the latch.
        // processor must rewrite T2C-H to enable setting of the interrupt flag.
        if (t2_oneshot_pending) {
            ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
            update_interrupt();
        }
        t2_oneshot_pending = 0;
    }
#endif

    void write(uint16_t reg, uint8_t data) {

        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) write: %s[%02x] = %02x\n", chip_id, reg_names[reg], reg, data);
        
        switch (reg) {

            case MB_6522_DDRA:
                ddra = data;
                break;
            case MB_6522_DDRB:
                ddrb = data;
                break;
            case MB_6522_ORA: case MB_6522_ORA_NH: // TODO: what the heck is NH here??
                // TODO: need to mask with DDRA
                ora = data;
                // Any access to the handshaking Port A register clears CA1;
                // ORA_NH deliberately does not perform that handshake.
                if (reg == MB_6522_ORA) {
                    ifr.bits.ca1 = 0;
                    update_interrupt();
                }
                break;
            case MB_6522_ORB:
                // TODO: instead of having this here, we could put this logic into the AY/Mockingboard code.
                // TODO: write to the connected chip callback - pass ORA and ORB 
                orb = data;
                // TODO: why is there a read below?
                /* 
                } else if ((data & 0b111) == 5) { // read from the specified register
                    // to read, the command is run through ORB, but the data goes back and forth on ORA/IRA.
                    //printf("attempt to read with cmd 05 from PSG, unimplemented\n");
                    ira = mb_d->mockingboard->read_register(chip, reg_num);
                    printf("read register: %02X -> %02X\n", reg_num, ira);
                } */
                break;
            case MB_6522_T1L_L: 
                /* 8 bits loaded into T1 low-order latches. This operation is no different than a write into REG 4 (2-42) */
                t1_latch = (t1_latch & 0xFF00) | data;
                break;
            case MB_6522_T1L_H:
                /* 6522 doc doesn't say it, but AppleWin and UltimaV clear timer1 interrupt flag when T1L_H is written. */
                ifr.bits.timer1 = 0;
                update_interrupt();       
            
                /* 8 bits loaded into T1 high-order latches. Unlike REG 4 OPERATION, no latch-to-counter transfers take place (2-42) */
                t1_latch = (t1_latch & 0x00FF) | (data << 8);
                break;
            case MB_6522_T1C_L:
                /* 8 bits loaded into T1 low-order latches. Transferred into low-order counter at the time the high-order counter is loaded (reg 5) */
                t1_latch = (t1_latch & 0xFF00) | data;
                break;
            case MB_6522_T1C_H:
                /* 8 bits loaded into T1 high-order latch. Also both high-and-low order latches transferred into T1 Counter. T1 Interrupt flag is also reset (2-42) */
                // write of t1 counter high clears the interrupt.
                
                ifr.bits.timer1 = 0;
                update_interrupt();
                t1_latch = (t1_latch & 0x00FF) | (data << 8);
                t1_counter = t1_latch;
                t1_oneshot_pending = 1;
                // The cycle in which T1C_H is written is a pure load cycle on
                // the real 6522 (no T1 decrement). Our cycle_handler runs
                // after this write within the same wall cycle, so suppress the
                // next decrement to match real hardware timing (mb-audit 11:03:00).
                t1_skip_next_decrement = true;
                break;

            case MB_6522_T2L_L:
                t2_latch = data;
                break;
            case MB_6522_T2C_H:
                t2_latch = (t2_latch & 0x00FF) | (data << 8);
                t2_counter = t2_latch;
                ifr.bits.timer2 = 0;
                update_interrupt();
                t2_oneshot_pending = 1;
                // Same load-cycle exemption as T1C_H above.
                t2_skip_next_decrement = true;
                break;

            case MB_6522_PCR:
                pcr = data;
                break;
            case MB_6522_SR:
                sr = data;
                break;
            case MB_6522_ACR:
                acr = data;
                break;
            
            case MB_6522_IFR:
                // for any bit set in data[6:0], clear the corresponding bit in the IFR.
                // Pg 2-49 6522 Data Sheet
                ifr.value &= ~(data & 0x7F);
                update_interrupt();
                break;

            case MB_6522_IER:
                // if bit 7 is a 0, then each 1 in bits 0-6 clears the corresponding bit in the IER
                // if bit 7 is a 1, then each 1 in bits 0-6 enables the corresponding interrupt.
                // Pg 2-49 6522 Data Sheet
                if (data & 0x80) {
                    ier.value |= data & 0x7F;
                } else {
                    ier.value &= ~data;
                }
                update_interrupt();             

                break;
        }
    }

    uint8_t read(uint16_t reg) {
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) read: %s[%02x] => ", chip_id, reg_names[reg], reg);
        uint8_t retval = 0xFF;

        switch (reg) {
            case MB_6522_DDRA:
                retval = ddra;
                break;
            case MB_6522_DDRB:
                retval = ddrb;
                break;
            case MB_6522_ORA: case MB_6522_ORA_NH: { 
                // TODO: what the heck is NH here??
                uint8_t a_in = ira & (~ddra);
                uint8_t a_out = ora & ddra;
                retval = a_out | a_in;
                if (reg == MB_6522_ORA) {
                    ifr.bits.ca1 = 0;
                    update_interrupt();
                }
                } 

                break;
            case MB_6522_ORB: {
                // Input bits come from the external pins (captured in irb),
                // not from ORB. On the Mockingboard, PB lines have pull-ups
                // so with DDRB=0 (all inputs) a reset read returns $FF
                // (mb-audit Test6522AfterReset subTest #8).
                uint8_t b_in  = irb & (~ddrb);
                uint8_t b_out = orb & ddrb;
                retval = b_out | b_in;
                } 

                break;
            case MB_6522_T1L_L:
                /* 8 bits from T1 low order latch transferred to mpu. unlike read T1 low counter, does not cause reset of T1 IFR6. */
                retval = t1_latch & 0xFF;
                break;
            case MB_6522_T1L_H:     
                /* 8 bits from t1 high order latch transferred to mpu */
                retval = (t1_latch >> 8) & 0xFF;
                break;
            case MB_6522_T1C_L:  {
                // IFR Timer 1 flag cleared by read T1 counter low. pg 2-42
                ifr.bits.timer1 = 0;
                update_interrupt();
                retval = t1_counter & 0xFF;
                break;
            }
            case MB_6522_T1C_H:    {  
                // IFR Timer 1 flag cleared by read T1 counter high. pg 2-42
                // read of t1 counter high DOES NOT clear interrupt; write does.

                retval = (t1_counter >> 8) & 0xFF;
                break;
            }
            case MB_6522_T2C_L: { 
                /* 8 bits from T2 low order counter transferred to mpu - t2 interrupt flag is reset. */
                ifr.bits.timer2 = 0;
                update_interrupt();

                retval = t2_counter & 0xFF;
                break;
            }
            case MB_6522_T2C_H: { 
                /* 8 bits from T2 high order counter transferred to mpu */
                retval = (t2_counter >> 8) & 0xFF;
                break;
            }
            case MB_6522_PCR:
                retval = pcr;
                break;
            case MB_6522_SR:
                retval = sr;
                break;
            case MB_6522_ACR:
                retval = acr;
                break;
            case MB_6522_IFR:
                retval = ifr.value;
                break;
            case MB_6522_IER:
                // if a read of this register is done, bit 7 will be "1" and all other bits will reflect their enable/disable state.
                retval = ier.value | 0x80;
                break;
        }
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("%02x\n", retval);
        return retval;
    }

    // Hardware RES behavior per the 6522 (and W65C22) datasheet: clears
    // ORA, ORB, DDRA, DDRB, ACR, PCR, IFR, IER, and SR. The T1/T2 latches
    // and counters are *not* affected (mb-audit Test6522AfterReset subTest
    // #2 explicitly requires T1L to retain its pre-reset value). The
    // datasheet also says reset "disables the timers" - model that by
    // clearing the pending/rollover state so any in-flight underflow that
    // would otherwise still fire after reset is discarded.
    void reset() {
        // Port registers and direction registers.
        ora  = 0x00;
        orb  = 0x00;
        ddra = 0x00;
        ddrb = 0x00;
        // After reset DDR{A,B}=0 (all inputs); with Mockingboard pull-ups
        // on Port A (from the AY bus) and Port B, reading I{R,R}{A,B}
        // should return $FF. mb-audit Test6522AfterReset subTests #8/#A.
        ira = 0xFF;
        irb = 0xFF;
        // Control / status registers.
        pcr  = 0x00;
        sr   = 0x00;
        acr  = 0x00;
        // Clear all pending interrupt flags and disable all interrupt
        // enables (bit 7 always reads back as 1 via the read path).
        ifr.value = 0;
        ier.value = 0;

        // Disable both timers. Without this, a T1 that was running in
        // continuous mode before reset (ACR bit 6 set) with a pending
        // one-shot flag would still fire its next underflow because reset
        // only clears ACR (switching it to one-shot) but leaves
        // t1_oneshot_pending latched. mb-audit Test6522AfterReset subTest
        // #4 requires IFR bits T1/T2 to read 0 after reset.
        t1_oneshot_pending = 0;
        t2_oneshot_pending = 0;
        t1_rollover = false;
        t2_rollover = false;
        t1_skip_next_decrement = false;
        t2_skip_next_decrement = false;

        update_interrupt(); // this reads the slot number and does the right IRQ thing.
    }


    void debug(DebugFormatter *df) {

        df->addLine("====== (%s) ======", chip_id);
        df->addLine("DDRA: %02X    DDRB: %02X    O/IRA: %02X/%02X  O/IRB: %02X/%02X",
            ddra, ddrb, ora, ira, orb, irb);
        df->addLine("T1L : %04X  T1C: %04X   T2L : %04X  T2C: %04X",
            t1_latch, t1_counter, t2_latch, t2_counter);
        df->addLine("SR  : %02X    ACR : %02X    PCR : %02X   IFR : %02X    IER: %02X", 
            sr, acr, pcr, ifr.value, ier.value|0x80);
//        df->addLine("T1 Int Timer: %08lld  T2 Int Timer: %08lld", t1_trigger_at, t2_trigger_at);
    }

    void debug_one() {
        printf("DDRA %02X DDRB %02X ORA %02X ORB %02X IRA %02X IRB %02X T1[L %04X C: %04X] T2[L %02X C %04X] SR %02X ACR %02X PCR %02X IFR %02X IER %02X\n",
            ddra, ddrb, ora, orb, ira, irb, t1_latch, t1_counter, t2_latch, t2_counter, sr, acr, pcr, ifr.value, ier.value);
    }

};
