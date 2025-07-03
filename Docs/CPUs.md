# CPUs



## 65c02

Very handy description of differences from 6502 to 65c02:

http://www.6502.org/tutorials/65c02opcodes.html

### Decimal Mode

ADC and SBC takes an extra cycle compared to 6502. Also sets flags correctly.


### INDIRECT address Mode

ADC, AND, CMP, EOR, LDA, ORA, SBC, STA (zp) . (no index, just indirect). All take 5 cycles.

### Additional address modes for BIT

* IMM
* ZP,X
* ABS,X

### DEC, INC - accumulator address mode

DEC and INC can now operate on accumulator

### JMP (abs,X) - new address mode

### New Instructions

* BRA - branch always
* PHX, PHY, PLX, PLY
* STZ - zp; STZ ZP,X; STZ ABS; STZ ABS,X
* TRB - ZP; ABS
* TSB - ZP; ABS

On Rockwell and WDC, you additionally have:

* BBR, BBS - ZP,REL - branch on bit reset or set
* RMB, SMB - zp

Address mode is "ZP,REL" - these are 3 byte instructions that combine checking a bit from a byte on ZP, then branching.

RMB SMB are r-m-w instructions.

### BRK (and irq, nmi, reset) difference

BRK/interrupt clears the D (decimal) flag on 65c02 (not on 6502).

### ASL, LSR, ROL, ROR ABS,X - cycle count difference

on 6502, always take 7 cycles. On 65c02, saves one cycle if page boundary not crossed.

### JMP (abs) bug

on 6502, JMP has a bug when low byte of operand is 0xFF - e.g. JMP ($12FF) takes the high byte from $1200 instead of $1300.
65c02 fixes this, and now takes 6 cycles instead of 5.
