#pragma once

#include <cstdint>
#include <functional>
#include "computer.hpp"
#include "device_reset_id.hpp"
#include "util/DebugFormatter.hpp"



/**
 * @class ResetController
 * @brief Centralized RESET management and notification for all emulated devices.
 *
 * The ResetController provides a mechanism for all emulated devices to assert or deassert
 * RESET independently of the CPU. Devices interact only with this controller—they do
 * not need any knowledge of the CPU's internal reset architecture or state.
 */

class ResetController {
    private:
    uint64_t reset_asserted = 0;
    
    // Callback to notify when IRQ state changes
    // Parameter is true if any IRQ is currently asserted
    std::function<void(uint64_t)> reset_receiver;

    computer_t *computer = nullptr;

    public:
    ResetController(computer_t *computer) { 
        reset_asserted = 0; 
        reset_receiver = nullptr; 
        this->computer = computer;
    };
    ~ResetController() { reset_receiver = nullptr; };

    inline void assert_reset(device_reset_id reset, bool assert) {
        uint64_t old = reset_asserted;

        if (assert) {
            reset_asserted |= (1 << reset);
        } else {
            reset_asserted &= ~(1 << reset);
        }

        if ((reset_asserted) && (!old)) { // if reset was NOT asserted, but now is, call reset() chain.
            computer->reset(false);
        }
        
        if (old != reset_asserted) { // on any change tell computer to change cpu reset state
            notify_reset_receiver();
        }
    }

    inline bool get_reset(device_reset_id reset) {
        return reset_asserted & (1 << reset);
    }

    // Register a callback to receive IRQ state updates
    // The callback receives true when any IRQ is asserted, false when all are cleared
    void register_reset_receiver(std::function<void(uint64_t)> receiver) {
        reset_receiver = std::move(receiver);
    }
    
    // Check if any IRQ is currently asserted
    inline bool any_reset_asserted() const {
        return reset_asserted != 0;
    }

    inline void clear_all_irqs() {
        uint64_t old = reset_asserted;
        reset_asserted = 0;
        if (old != reset_asserted) {
            notify_reset_receiver();
        }
    }

    protected:
    // Call this after IRQ state changes to notify the receiver
    void notify_reset_receiver() {
        if (reset_receiver) {
            reset_receiver(reset_asserted);
        }
    }

};