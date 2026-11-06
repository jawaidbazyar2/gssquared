/*
 *   Connect the Second Sight card model to GSSquared's Apple IIgs machine.
 *   The card's Z8S180 runs its firmware in SSCard; this adapter supplies the
 *   ROM, observes Apple bus accesses, saves configuration memory and attaches
 *   the monitor. All callbacks here run on the application's main thread.
 *
 *   Mega II is the IIgs chip supplying the slow Apple II memory and slot bus.
 *   An IRQ is an interrupt request to the Apple CPU. EEPROM is electrically
 *   erasable memory that keeps the card's settings after power-off.
 *   Sources: Second Sight manual (installation and monitor choice), Apple
 *   IIgs Technical Note 68 (Mega II bank signal), and ROM_BEEF.asm Reset at
 *   https://github.com/jawaidbazyar2/secondsight . Resource loading, file
 *   persistence and callback lifetime are application policy.
 */

#pragma once

#include "gs2.hpp"
struct computer_t;
// Install the card for computer in slot. The machine shutdown hook owns its
// lifetime. Only an Apple IIgs supplies the required bus observer.
void init_secondsight_z180(computer_t *computer, SlotType_t slot);
