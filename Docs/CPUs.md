# CPUs

## 65c02

Very handy description of differences from 6502 to 65c02:

http://www.6502.org/tutorials/65c02opcodes.html

### Decimal Mode

[ ] ADC and SBC takes an extra cycle compared to 6502. Also sets flags correctly.  

the cycle is no biggie. Setting the flags, need to research.

### INDIRECT address Mode

[x] ADC, AND, CMP, EOR, LDA, ORA, SBC, STA (zp) . (no index, just indirect). All take 5 cycles.

check clock cycle counts on these! I think STA is wrong.

### Additional address modes for BIT

[x] IMM  
[x] ZP,X  
[x] ABS,X  

### DEC, INC - accumulator address mode

[x] DEC and INC can now operate on accumulator  

### JMP (abs,X) - new address mode

### New Instructions

[x] BRA - branch always  
[x] PHX, PHY, PLX, PLY   
[x] STZ - zp; STZ ZP,X; STZ ABS; STZ ABS,X  
[x] TRB - ZP; ABS  
[x] TSB - ZP; ABS  

On Rockwell and WDC, you additionally have:

[ ] BBR, BBS - ZP,REL - branch on bit reset or set  
[ ] RMB, SMB - zp  

Address mode is "ZP,REL" - these are 3 byte instructions that combine checking a bit from a byte on ZP, then branching.

BUT these chips were basically never used on an Apple IIe. The "Enhanced" chip was the NCR 65C02. 

RMB SMB are r-m-w instructions.

### BRK (and irq, nmi, reset) difference

[x] BRK/interrupt clears the D (decimal) flag on 65c02 (not on 6502).  

### ASL, LSR, ROL, ROR ABS,X - cycle count difference

[ ] on 6502, always take 7 cycles. On 65c02, saves one cycle if page boundary not crossed.  

### JMP (abs) bug

on 6502, JMP has a bug when low byte of operand is 0xFF - e.g. JMP ($12FF) takes the high byte from $1200 instead of $1300.
[ ] 65c02 fixes this, and now takes 6 cycles instead of 5.  


## Invalid opcodes

[ ] The remaining invalid opcodes each consume a certain number of bytes and cycles, but are otherwise NOPs. I have partially implemented these. The test runs, however, it will be important to have these fetch the right number of bytes and use the right number of cycles. So, I need to generate like 64 of these.

