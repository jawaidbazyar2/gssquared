#pragma once

#include <cstdint>

#include "debug.hpp"
#include "util/InterruptController.hpp"
#include "util/ClockRail.hpp"
#include "NClock.hpp"

#define MB_6522_DDRA 0x03
#define MB_6522_DDRB 0x02
#define MB_6522_ORA  0x01
#define MB_6522_ORB  0x00

#define MB_6522_T1C_L 0x04
#define MB_6522_T1C_H 0x05
#define MB_6522_T1L_L 0x06
#define MB_6522_T1L_H 0x07

#define MB_6522_T2L_L 0x08
#define MB_6522_T2C_L 0x08
#define MB_6522_T2C_H 0x09
#define MB_6522_SR 0x0A
#define MB_6522_ACR 0x0B
#define MB_6522_PCR 0x0C
#define MB_6522_IFR 0x0D
#define MB_6522_IER 0x0E
#define MB_6522_ORA_NH 0x0F

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
    //uint16_t t1_counter;  
    uint8_t t2_latch;
    //uint16_t t2_counter;
    uint16_t t1_oneshot_pending = 0;
    uint16_t t2_oneshot_pending = 0;

    uint8_t slot;
    uint8_t chip;
    
    uint64_t t1_set_cycles;
    uint64_t t2_set_cycles;
    uint16_t t1_set_value;
    uint16_t t2_set_value;
    bool t1_continuous = false;
    
    uint64_t t1_trigger_at;
    uint64_t t2_trigger_at;
    
    uint64_t t1_instanceID;
    uint64_t t2_instanceID;

    const char *chip_id;
    InterruptController *irq_control = nullptr;
    NClock *clock = nullptr;
    VidRail *vid = nullptr;

    const char *reg_names[16] = {
        "ORB/IRB", "ORA/IRA", "DDRB", "DDRA",
        "T1C_L", "T1C_H","T1L_L", "T1L_H",
        "T2L_L", "T2C_H", "SR", "ACR",
        "PCR", "IFR", "IER", "ORANH"
    };
    
public:
    N6522(const char *chip_id, NClock *clock, InterruptController *irq_controller, VidRail *vid,
        uint8_t slot = 0, uint8_t chip = 0) : chip_id(chip_id), slot(slot), chip(chip), clock(clock), irq_control(irq_controller), vid(vid) {
        if (chip_id == nullptr) {
            throw std::invalid_argument("N6522: chip_id must be provided and non-null");
        }
        if ((!clock) || (!irq_controller) || (!vid)) {
            throw std::invalid_argument("N6522: clock, irq_controller, and vid must be provided and non-null");
        }
        if (slot < 0 || slot > 7) {
            throw std::invalid_argument("N6522: slot must be between 0 and 7");
        }
        if (chip < 0 || chip > 1) {
            throw std::invalid_argument("N6522: chip must be between 0 and 1");
        }
        t1_latch = 0;
        t2_latch = 0;
        ddra = 0x00;
        ddrb = 0x00;
        ora = 0x00;
        orb = 0x00;
        ira = 0x00;
        irb = 0x00;
        ifr.value = 0;
        ier.value = 0;
        acr = 0;
        pcr = 0;
        sr = 0;

        t1_set_cycles = 0;
        t2_set_cycles = 0;
        t1_set_value = 0;
        t2_set_value = 0;
        t1_continuous = false;
        t1_trigger_at = 0;
        t2_trigger_at = 0;
        // Calculate instance ID for each timer based on slot and chip.
        t1_instanceID = 0x10000000 | (slot << 8) | chip;
        t2_instanceID = 0x10010000 | (slot << 8) | chip;
    };
    //~W6522();

    inline uint64_t get_clock_cycles() { return clock->get_vid_cycles(); }

    // Latch a byte onto the input register of Port A. Used by the
    // Mockingboard bridge when the paired AY drives the data bus on a read.
    inline void set_ira(uint8_t value) { ira = value; }

    // Accessors needed by the Mockingboard bridge to derive the AY bus
    // state (post-DDR) from this generic 6522 without leaking write access.
    inline uint8_t get_ora()  const { return ora;  }
    inline uint8_t get_orb()  const { return orb;  }
    inline uint8_t get_ddra() const { return ddra; }
    inline uint8_t get_ddrb() const { return ddrb; }

    void set_clock(NClock *clock) {
        this->clock = clock;
    }

    void set_irq_controller(InterruptController *irq_controller) {
        this->irq_control = irq_controller;
    }

    void set_event_timer(VidRail *vid) {
        this->vid = vid;
    }

    void schedule_t1(uint64_t fromnow) {
        t1_trigger_at = get_clock_cycles() + fromnow + 2;
        vid->schedule(t1_trigger_at, mb_t1_timer_callback, t1_instanceID, this);
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) scheduled %08llx at %08lld for timer1\n", chip_id, t1_instanceID, t1_trigger_at);
    }

    void schedule_t2(uint64_t fromnow) {
        t2_trigger_at = get_clock_cycles() + fromnow + 2;
        vid->schedule(t2_trigger_at, mb_t2_timer_callback, t2_instanceID, this);
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) scheduled %08llx at %08lld for timer2\n", chip_id, t2_instanceID, t2_trigger_at);
    }
    /*
TODO: 

does clearing IER bit cause the corresponding IFR bit to be cleared?
the docs say only certain other operations clear it. ah, but IER would
gate whether the system generates an IRQ out for it.
*/

    void update_interrupt() {
        // calculate the IFR bit 7.
                
        bool irq = ((ifr.value & ier.value) & 0x7F) > 0;
        
        // set bit 7 of IFR to the result.
        ifr.bits.irq = irq;

        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("%1lld (%s) interrupt set %d [ T1 E%d/F%d ] [ T2 E%d/F%d ]:\n", get_clock_cycles(), chip_id, irq, ier.bits.timer1, ifr.bits.timer1, ier.bits.timer2, ifr.bits.timer2);
        irq_control->set_irq((device_irq_id)chip, irq);
    }

    static void mb_t1_timer_callback(uint64_t instanceID, void *user_data) {
        N6522 *n6522 = (N6522 *)user_data;
        uint64_t cycles = n6522->get_clock_cycles();

        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) T1 callback: %08llx\n", n6522->chip_id, instanceID);
        
        if (n6522->t1_continuous) { // continuous mode
            n6522->ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
            n6522->update_interrupt();
            
            n6522->schedule_t1(n6522->t1_latch);
        } else {         // one-shot mode
            // if a T1 oneshot was pending, set interrupt status.
            if (n6522->t1_oneshot_pending) {
                n6522->ifr.bits.timer1 = 1; // "Set by 'time out of T1'"
                n6522->update_interrupt();
            }
            // We don't schedule a next interrupt - that is only done when writing to T1C-H.
            // and we don't reset the counter to the latch, we continue decrementing from 0. <- this is wrong. counter -should- reset.
            n6522->t1_oneshot_pending = 0;
        }
    }

    static void mb_t2_timer_callback(uint64_t instanceID, void *user_data) {
        N6522 *n6522 = (N6522 *)user_data;
        
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) T2 callback: %08llx\n", n6522->chip_id, instanceID);
        
        // "after timing out, the counter will continue to decrement."
        // so do NOT reset the counter to the latch.
        // processor must rewrite T2C-H to enable setting of the interrupt flag and reset of counter
        // to latch value.
        if (n6522->t2_oneshot_pending) {
            n6522->ifr.bits.timer2 = 1; // "Set by 'time out of T2'"
            n6522->update_interrupt();
        }
        // We don't schedule a next interrupt - that is only done when writing to T1C-H.
        // and we don't reset the counter to the latch, we continue decrementing from 0. <- this is wrong. counter -should- reset.
        n6522->t2_oneshot_pending = 0;
    }

    void write(uint16_t reg, uint8_t data) {
        
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) write: %s[%02x] = %02x\n", chip_id, reg_names[reg], reg, data);
        
        uint64_t cycles = get_clock_cycles();

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
                break;
            case MB_6522_ORB:
                orb = data;
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
                {
                /* 8 bits loaded into T1 high-order latch. Also both high-and-low order latches transferred into T1 Counter. T1 Interrupt flag is also reset (2-42) */
                // write of t1 counter high clears the interrupt.
                ifr.bits.timer1 = 0;
                update_interrupt();
                t1_latch = (t1_latch & 0x00FF) | (data << 8);
                
                t1_set_value = t1_latch;
                t1_set_cycles = cycles;
                t1_continuous = (acr & 0x40) ? true : false;

                // in either mode, set 
                if (!t1_continuous) t1_oneshot_pending = 1; // only set this if we're not in continuous mode.
                // we don't need to do this if interrupts are disabled.
                if (ier.bits.timer1) {
                    schedule_t1(t1_latch);
                }
                }
                break;

            case MB_6522_T2C_L:
                t2_latch = (t2_latch & 0xFF00) | data;
                break;
            case MB_6522_T2C_H: {
                t2_latch = (t2_latch & 0x00FF) | (data << 8);

                ifr.bits.timer2 = 0;
                update_interrupt();
                
                t2_set_value = t2_latch;
                t2_set_cycles = cycles;

                t2_oneshot_pending = 1;
                // we don't need to do this if interrupts are disabled.
                if (ier.bits.timer2) {
                    schedule_t2(t2_latch);
                }
                }
                break;

            case MB_6522_PCR:
                pcr = data;
                break;
            case MB_6522_SR:
                sr = data;
                break;
            case MB_6522_ACR:
            // TODO: do we need to do something if we switch from continuous to one-shot mode?
                acr = data;
                break;
            case MB_6522_IFR:
                {
                    uint8_t wdata = data & 0x7F;
                    // for any bit set in wdata, clear the corresponding bit in the IFR.
                    // Pg 2-49 6522 Data Sheet
                    ifr.value &= ~wdata;
                    update_interrupt();
                }
                break;
            case MB_6522_IER:
                {
                    // if bit 7 is a 0, then each 1 in bits 0-6 clears the corresponding bit in the IER
                    // if bit 7 is a 1, then each 1 in bits 0-6 enables the corresponding interrupt.
                    // Pg 2-49 6522 Data Sheet
                    if (data & 0x80) {
                        ier.value |= data & 0x7F;
                    } else {
                        ier.value &= ~data;
                    }
                    update_interrupt();
                    
                    // Set or reset the next interrupt time, if the given interrupt is enabled.
                    if (ier.bits.timer1) {
                        // if in continuous mode or in a one-shot, set the next interrupt time.
                        if ((acr & 0x40) || t1_oneshot_pending) {
                            schedule_t1(read_t1_counter());
                        }
                    } else {
                        vid->cancel(t1_instanceID);
                    }

                    if (ier.bits.timer2) {
                        if (t2_oneshot_pending) {
                            schedule_t2(read_t2_counter());
                        }
                    } else {
                        vid->cancel(t2_instanceID);
                    }
                }
                break;
        }
    }

    inline uint16_t read_t1_counter() {
        uint64_t cycles = get_clock_cycles();

        if (t1_continuous) {
            //printf("   t1_set_value: %d  cycles: %08lld    t1_set_cycles: %08lld\n", t1_set_value, cycles, t1_set_cycles);
            uint64_t elapsed = cycles - t1_set_cycles;
            uint32_t period = (uint32_t)t1_set_value + 2;   // N -> ... -> 0 -> FFFF -> reload
            uint32_t phase = (uint32_t)(elapsed % period);

            if (phase <= t1_set_value) {
                return (uint16_t)(t1_set_value - phase);
            }

            return 0xFFFF;
        } else {
            // in continuous mode, use modulus of the latch value.
            return (t1_set_value - (cycles - t1_set_cycles)) & 0xFFFF;
        }
    }

    inline uint16_t read_t2_counter() {
        // treat latch of 0 as 65535.
        uint64_t cycles = get_clock_cycles();
        
        // t2 is one-shot mode, so we don't need to worry about modulus.
        return (t2_set_value - (cycles - t2_set_cycles)) & 0xFFFF;
    }

    uint8_t read(uint16_t reg) {

        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("read %s: %s[%02x] => ", chip_id, reg_names[reg], reg);
        uint8_t retval = 0xFF;
        switch (reg) {
            case MB_6522_DDRA:
                retval = ddra;
                break;
            case MB_6522_DDRB:
                retval = ddrb;
                break;
            case MB_6522_ORA: case MB_6522_ORA_NH: { // TODO: what the heck is NH here??
                uint8_t a_in = ira & (~ddra);
                uint8_t a_out = ora & ddra;
                retval = a_out | a_in;
                } 
                break;
            case MB_6522_ORB: {
                uint8_t b_in = orb & (~ddrb);
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
            case MB_6522_T1C_L:  {  // IFR Timer 1 flag cleared by read T1 counter low. pg 2-42
                ifr.bits.timer1 = 0;
                update_interrupt();
                uint64_t c = read_t1_counter();
                retval = c & 0xFF;
                break;
            }
            case MB_6522_T1C_H:    {  // IFR Timer 1 flag cleared by read T1 counter high. pg 2-42
                // read of t1 counter high DOES NOT clear interrupt; write does.
                uint64_t c = read_t1_counter();
                retval = (c >> 8) & 0xFF;
                break;
            }
            case MB_6522_T2C_L: { /* 8 bits from T2 low order counter transferred to mpu - t2 interrupt flag is reset. */
                ifr.bits.timer2 = 0;
                update_interrupt();
                uint64_t c = read_t2_counter();
                retval = (c) & 0xFF;
                break;
            }
            case MB_6522_T2C_H: { /* 8 bits from T2 high order counter transferred to mpu */
                uint64_t c = read_t2_counter();
                retval = (c >> 8) & 0xFF;            
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
        if (DEBUG(DEBUG_MOCKINGBOARD)) printf("(%s) read: %s[%02x] => %02x\n", chip_id, reg_names[reg], reg, retval);
        return retval;
    }

    // no need to repeat the logic here, just call write with the appropriate register and data.
    void reset() {
        write(MB_6522_ACR, 0);
        write(MB_6522_IFR, 0b0'1111111);
        write(MB_6522_IER, 0b0'1111111);
        update_interrupt();
    }

    void debug(DebugFormatter *df) {

        df->addLine("====== (%s) ======", chip_id);
        df->addLine("DDRA: %02X    DDRB: %02X    O/IRA: %02X/%02X  O/IRB: %02X/%02X",
            ddra, ddrb, ora, ira, orb, irb);
        df->addLine("T1L : %04X  T1C: %04X   T2L : %04X  T2C: %04X",
            t1_latch, read_t1_counter(), t2_latch, read_t2_counter());
        df->addLine("SR  : %02X    ACR : %02X    PCR : %02X   IFR : %02X    IER: %02X", 
            sr, acr, pcr, ifr.value, ier.value|0x80);
        df->addLine("T1 Int Timer: %08lld  T2 Int Timer: %08lld", t1_trigger_at, t2_trigger_at);
    }

    void debug_one() {
        printf("(%s) DDRA %02X DDRB %02X ORA %02X ORB %02X IRA %02X IRB %02X T1[L %04X C: %04X] T2[L %02X C %04X] SR %02X ACR %02X PCR %02X IFR %02X IER %02X\n",
            chip_id,
            ddra, ddrb, ora, orb, ira, irb, t1_latch, read_t1_counter(), t2_latch, read_t2_counter(), sr, acr, pcr, ifr.value, ier.value);
    }
};
