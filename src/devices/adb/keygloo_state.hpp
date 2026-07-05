#pragma once

class KeyGloo;
class MMU_II;
class computer_t;
class InterruptController;
class ResetController;

struct keygloo_state_t {
    KeyGloo *kg = nullptr;
    MMU_II *mmu = nullptr;
    computer_t *computer = nullptr;
    InterruptController *irq_control = nullptr;
    ResetController *reset_control = nullptr;
};
