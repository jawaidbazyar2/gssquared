#pragma once

#include <cstdint>
#include <functional>

#include "device_irq_id.hpp"
#include "util/DebugFormatter.hpp"

/**
 * @class InterruptController
 * @brief Centralized IRQ management and notification for all emulated devices.
 *
 * The InterruptController provides a mechanism for all emulated devices to assert or deassert
 * interrupts (IRQ) independently of the CPU. Devices interact only with this controller—they do
 * not need any knowledge of the CPU's internal interrupt architecture or state.
 *
 * Each supported device gets a unique IRQ ID. Devices use `assert_irq()` and `deassert_irq()`
 * to update their interrupt line state. The controller tracks all IRQs as a bitfield and will
 * notify a registered CPU-side callback whenever the overall IRQ state changes (any transition
 * between 0 and 1 or vice versa).
 * 
 * This architecture enables modular device emulation—devices are fully decoupled from the CPU
 * interrupt logic and never call CPU functions directly. The InterruptController takes care
 * of centralizing state and acts as the bridge to the emulated CPU, or any registered IRQ
 * receiver.
 *
 * Typical usage:
 *   - Devices assert/deassert their own IRQ via the InterruptController.
 *   - The CPU registers a callback to receive edge notifications for any IRQ state changes.
 *   - The controller provides programmatic methods to query current IRQ line status.
 *
 */

class InterruptController {
    private:
    uint64_t irq_asserted = 0;
    
    // Callback to notify when IRQ state changes
    // Parameter is true if any IRQ is currently asserted
    std::function<void(bool)> irq_receiver;

    public:
    InterruptController() { 
        irq_asserted = 0; 
        irq_receiver = nullptr; 
    };
    ~InterruptController() { irq_receiver = nullptr; };

    inline void assert_irq(device_irq_id irq) {
        uint64_t old = irq_asserted;
        irq_asserted |= (1 << irq);
        if (old != irq_asserted) {
            notify_irq_receiver();
        }
    }

    inline void deassert_irq(device_irq_id irq) {
        uint64_t old = irq_asserted;
        irq_asserted &= ~(1 << irq);
        if (old != irq_asserted) {
            notify_irq_receiver();
        }
    }

    inline void set_irq(device_irq_id irq, bool assert) {
        uint64_t old = irq_asserted;

        if (assert) {
            irq_asserted |= (1 << irq);
        } else {
            irq_asserted &= ~(1 << irq);
        }

        if (old != irq_asserted) {
            notify_irq_receiver();
        }
    }

    inline bool get_irq(device_irq_id irq) {
        return irq_asserted & (1 << irq);
    }

    // Register a callback to receive IRQ state updates
    // The callback receives true when any IRQ is asserted, false when all are cleared
    void register_irq_receiver(std::function<void(bool)> receiver) {
        irq_receiver = std::move(receiver);
    }
    
    // Check if any IRQ is currently asserted
    inline bool any_irq_asserted() const {
        return irq_asserted != 0;
    }

    inline void clear_all_irqs() {
        uint64_t old = irq_asserted;
        irq_asserted = 0;
        if (old != irq_asserted) {
            notify_irq_receiver();
        }
    }
    
    DebugFormatter *debug_irq() {
        DebugFormatter *f = new DebugFormatter();
        f->addLine("IRQ: %08llX", irq_asserted);
        if (irq_asserted & (1<<IRQ_ID_ADB_DATAREG)) f->addLine(" |- ADB Data Reg");
        if (irq_asserted & (1<<IRQ_ID_KEYGLOO)) f->addLine(" |- KeyGloo");
        if (irq_asserted & (1<<IRQ_ID_VGC)) f->addLine(" |- VGC");
        if (irq_asserted & (1<<IRQ_ID_SOUNDGLU)) f->addLine(" |- SoundGlu");
        return f;
    }
    
    protected:
    // Call this after IRQ state changes to notify the receiver
    void notify_irq_receiver() {
        if (irq_receiver) {
            irq_receiver(irq_asserted);
        }
    }

};