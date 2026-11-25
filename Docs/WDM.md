# Users of the WDM Opcode

This is to document users of WDM $XX opcodes / signature bytes.

| Signature | Emulator | Description |
|-|-|-|
| C7 | KEGS | Call to smartport emulation (C and V determine function) |
| FF | GSPlus / GSSquared | Interface with Host FST logic |
| ?? | Michael Guidero | debugging |

Enhanced-mode length classes (see [65816MMUInstructions.md](65816MMUInstructions.md)): `$C0–$FF` 2-byte, `$80–$BF` 3-byte, `$00–$7F` 4-byte. `$FF` stays 2-byte; MMU mnemonics do not use it.


## Proposed

| Signature | Emulator | Description |
|-|-|-|
| $SS $00 | KEGS | Emus would skip the extra BRK after this |
| $EA | KEGS | performs CLV |
| $EA, $00 | KEGS | Returns emulator info |

WDM $EA $00 returns emulator info: a string describing the emulator name is written to the 24-bit address DBANK.ACC, X=size of that buffer.  On output, X=size of string written, ACC is emulator version as BCD so $0123 is version 1.23, and Y=feature support, where bit 0 means the $C06C-$C06F timer is available (the kegs 32-bit cycle counter).

