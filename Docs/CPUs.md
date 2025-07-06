# CPUs

## General

We should use the constexpr stuff to create trace and non-trace versions of the CPU. So we're instantiating a LOT of different CPU types but so what? We have all the memory in the world.

## Varieties

the way the code is now, callers can instantiate a wide variety of 6502 CPU cores. Here are the main selectors:

* NMOS 6502 vs 65C02
* NCR 65C02 vs Rockwell/WDC 65C02

Additional selectors to contemplate:

* Trace vs No Trace

This would enable or disable trace support at compile-time for maximum performance.

* Cycle-Accurate Video vs not

Enable or disable hook to call for cycle-accurate video support routine.

## 65c02

Very handy description of differences from 6502 to 65c02:

http://www.6502.org/tutorials/65c02opcodes.html

### Performance

Performance should stay high since we are using the template-based system to select processor criteria.

### Decimal Mode

[x] ADC and SBC takes an extra cycle compared to 6502. Also sets flags correctly.  

the cycle is no biggie. Setting the flags, need to research.

http://forum.6502.org/viewtopic.php?t=254&sid=daf0a8f089eb7b90f606bbd6193eb634

"N reflects the high bit (even though it's not really relevant to what might be considered negative numbers in decimal), and Z is set if all resulting bits are 0 and cleared otherwise (just as in binary mode). C also works correctly on the 65c02 just as you'd expect, but V is not valid for decimal arithmetic."

So, I partially implemented flag handling in decimal mode on the 6502. So 65c02 inherits that, and, the test passes so I guess I did that right. I could, however, change the implementation so the flags are undefined in 6502 mode. (Or, use a formula that is wrong).

https://www.nesdev.org/wiki/Visual6502wiki/6502DecimalMode

http://www.6502.org/tutorials/decimal_mode.html#B

So I think I'm basically correct already. But, need to run the other tester (8-bit-shack)

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


### BRK (and irq, nmi, reset) difference

[x] BRK/interrupt clears the D (decimal) flag on 65c02 (not on 6502).  


### ASL, LSR, ROL, ROR ABS,X - cycle count difference

[ ] on 6502, always take 7 cycles. On 65c02, saves one cycle if page boundary not crossed.  

### JMP (abs) bug

on 6502, JMP has a bug when low byte of operand is 0xFF - e.g. JMP ($12FF) takes the high byte from $1200 instead of $1300.

[ ] 65c02 fixes this, and now takes 6 cycles instead of 5.  

Our implementation is already correct. So we need to implement a broken one for NMOS 6502.

## Rockwell / WDC 65c02 Additions

In addition to the changes in the "base" (i.e. NCR) 65c02 chip, On Rockwell and WDC only, you also have:

[ ] BBR, BBS - ZP,REL - branch on bit reset or set  
[ ] RMB, SMB - zp  

Address mode is "ZP,REL" - these are 3 byte instructions that combine checking a bit from a byte on ZP, then branching.

However, there is also documentation out there that the Rockwell chips BBR etc are TWO byte instructions, that test bits in the accumulator, as opposed to in memory. 

The 65c02 test suite and as65 assembler assume the 3-byte variety of instruction. It is unclear if these work anywhere. Maybe ask on 6502.org.

BUT these chips were basically never used on an Apple IIe. The "Enhanced" chip was the NCR 65C02. 

RMB SMB are r-m-w instructions.


## Invalid opcodes

[x] The remaining invalid opcodes each consume a certain number of bytes and cycles, but are otherwise NOPs. I have partially implemented these. The test runs, however, it will be important to have these fetch the right number of bytes and use the right number of cycles. So, I need to generate like 64 of these.

the NCR65c02 had the following:

All are NOPs:
| Opcode | Bytes | Cycles | handled? |
|-|-|-|-|
| 02, 22, 42, 62, 82, C2, E2| 2 | 2 | Y |
| X3,X7,XB,XF | 1 | 1 | Y | 
| 44 | 2 | 3 | Y |
| 54, D4, F4 | 2 | 4 | Y |
| 5C | 3 | 8 | Y |
| DC, FC | 3 | 4 | Y |


## 65816

Test suite:

https://github.com/SingleStepTests/ProcessorTests/tree/main/65816

The 816 has FIVE combinations of modes.

| Mode | A | X |
|-|-|-|
| Emulation | 8 | 8 |
| Native | 8 | 8 |
| Native | 8 | 16 |
| Native | 16 | 8 |
| Native | 16 | 16 |

There are a number of subtleties and variations between Emulation and Native mode. Emulation is not precisely the same as a 65c02, because of handling data and program banks etc.

We could handle native as one mode, but, there is going to be a lot of testing of m/x flags and it will not be efficient. I think using constexpr and building five versions is memory inefficient, but, performance efficient.  The code will still look pretty normal.

Now, it probably makes sense to have this be a separate code file from the 6502/65c02, as there will be a great many differences. Each address mode handler is two variations (8 / 16 bit). Probably want to make all add/subtract/compare/etc etc into two versions (8-bit and 16-bit).

Any bugs that need fixing in the 02/c02 should be few and far between at this point and easily migrated to the 816 code.
