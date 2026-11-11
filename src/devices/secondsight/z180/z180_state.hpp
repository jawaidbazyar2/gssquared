// license:BSD-3-Clause
// copyright-holders:Juergen Buchmueller

/*
 *   Stored registers of Second Sight's Zilog Z8S180, included inside Z180.
 *   The CPU owns all fields and its execution thread is their sole writer.
 *   Opcode handlers change programmer registers; the internal-port decoder
 *   changes timers, direct memory access (DMA) and memory mapping. Save/load
 *   retain them along
 *   with the separate bus continuation state.
 *
 *   PC is the program counter; SP the stack pointer; AF the accumulator/flags;
 *   BC, DE and HL general register pairs; IX/IY address index registers.
 *   Suffix 2 selects the alternate register set. R counts refresh fetches;
 *   R2 retains its separately writable high bit. I is the interrupt-vector
 *   high byte, IM the interrupt mode, and IFF1/2 the interrupt-enable flags.
 *   HALT=0 runs, 1 repeats HALT timing, and 2 is the SLP sleep instruction.
 *
 *   PHI is the processor clock. PRT means programmable reload timer; FRC is
 *   the free-running divider counter. DMA means direct memory access: each
 *   channel copies bytes without executing a CPU instruction for each byte.
 *   MMU means memory management unit, mapping 16-bit to 20-bit addresses.
 *   A latch is a saved value held until a later read or signal replaces it.
 *   ASCI is asynchronous serial communication; CSI/O uses a separate clock.
 *   IRQ means interrupt request; NMI is the non-maskable interrupt. TOUT is
 *   timer output, sharing the A18 address pin. TCR controls the timers; TMDR
 *   holds a timer count. WAIT requests an extra bus clock; TRAP handles an
 *   undefined instruction. INT0-2 are the maskable interrupt pins.
 *
 *   Sources: Zilog UM005004-0918 pp. 55-64 (MMU), 65-115 (interrupts/DMA),
 *   157-164 (timers), 173-179 (registers) and 293-302 (internal port map).
 */

Z180Register PREPC, PC, SP, AF, BC, DE, HL, IX, IY{};
Z180Register AF2, BC2, DE2, HL2{};
uint8_t R, R2, IFF1, IFF2, HALT, IM, I{};
// Reading a timer low byte holds its high byte for a consistent two-byte
// read. TCR/TMDR read order also clears timeout flags (UM0050 pp. 159-162).
uint8_t tmdr_latch{};          // flag latched TMDR0H, TMDR1H values
uint8_t read_tcr_tmdr[2]{};    // records a TCR or TMDR read for the two-read acknowledgement sequence
uint32_t iol{};                // I/O line status bits
Z180Word tmdr[2]{};            // PRT data register ch 0-1
Z180Word rldr[2]{};            // PRT reload register ch 0-1
uint8_t tcr = 0;               // PRT control register
bool prt1_timeout_odd = false; // includes timeouts while the pin is A18
bool tout_level = true;
uint8_t frc{};          // free running counter (also time base for ASCI, CSI/O & PRT)
uint8_t frc_prescale{}; // divide CPU clock by 10
bool bus_released = false;
Z180Register dma_sar0{};       // DMA source address register ch 0
Z180Register dma_dar0{};       // DMA destination address register ch 0
Z180Word dma_bcr[2]{};         // DMA byte register ch 0-1
Z180Register dma_mar1{};       // DMA memory address register ch 1
Z180Register dma_iar1{};       // DMA I/O address register ch 1
uint8_t dstat{};               // DMA status register
uint8_t dmode{};               // DMA mode register
uint8_t dcntl{};               // DMA/WAIT control register
uint8_t il{};                  // INT vector low register
uint8_t itc{};                 // INT/TRAP control register
uint8_t rcr{};                 // refresh control register
uint8_t mmu_cbr{};             // MMU common base register
uint8_t mmu_bbr{};             // MMU bank base register
uint8_t mmu_cbar{};            // MMU common/bank area register
uint8_t omcr{};                // operation mode control register
uint8_t iocr{};                // I/O control register
// Cache one translated base per 4-KiB logical page; rebuild on CBR/BBR/CBAR writes.
uint32_t mmu[16]{};            // MMU address translation
uint8_t tmdrh[2]{};            // latched TMDR0H and TMDR1H values
uint16_t tmdr_value[2]{};      // live down-counter values for PRT0 and PRT1
uint8_t nmi_state{};           // NMI line state
uint8_t nmi_pending{};         // NMI pending
uint8_t irq_state[3]{};        // IRQ line states (INT0,INT1,INT2)
uint8_t int_pending[11 + 1]{}; // interrupt pending
uint8_t after_EI{};            // defer interrupts through the instruction after EI (p. 69)
// Effective address of the current indexed instruction, retained across helper calls.
uint32_t ea{};
