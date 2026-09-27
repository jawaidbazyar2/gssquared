# GSSquared Test Suites Status

In GSSquared development, I frequently utilized what is known as test-driven development. Test programs are written to exercise hardware, record behavior, and then the emulator is written to also match the recorded results.

Summary:
At this time there are no known issues with 65C02 or 65816 emulation; N6502 lacks support for the "undocumented opcodes".

IIgs needs support for PAL timing and 14/16MHz mode switching effects;
NTSC/PAL colorburst handling needs improvement; 

IIgs IWM needs work.

With the exception of the N6502 above, this is cosmetic and none causes software to crash. 


## Tests

| Test | Platforms | Pass/Fail | Notes |
|-|-|-|-|
| [6502IRQ](https://github.com/jawaidbazyar2/apple2validation/blob/main/Test_6502IRQ/Test_6502IRQ.md) | IIe | ✅ | Tests exact 6502 IRQ timing inside instruction execution |
| Textfunk | IIgs | ✅ | There is still a minor timing discrepancy that affects the border coloring but not display of the pictures |
| [mb-audit](https://github.com/tomcw/mb-audit) | IIe | ✅ | Tom Charlesworth amazing Mockingboard Test |
| Built-in Diag | IIe | ✅ | Passes |
| ROM 01 Built-in Diag | IIgs | ✅ | Passes |
| ROM 03 Built-in Diag | IIgs | ❌ | Fails due to missing ADB Sticky Keys |
| 6502_65C02_functional_tests | II+, IIe, IIe Enh | ✅ | 100% pass |
| [snes-tests](https://github.com/gilyon/snes-tests) | IIgs | ✅ | A pretty good SNES emulation community 65816 test |
| cycletest | IIe, IIgs | ✅ | I wrote this test, it passes 100% but it missed a few instructions, needs an update |
| IWMtest | IIgs | ✅❌ | Ian Brumby test suite for IWM - we pass about half |
| [Zellyn Audit](https://zellyn.com/a2audit/) | ✅ | IIe MMU / lang card / aux bank tests |
| Apple II+ Diagnostic Disk | II+ | ✅ | Apple tool for II+ era, disks, language card, RAM test, joystick |
| Apple IIgs Diagnostic 3.1 | IIgs | ✅❌ | Fails SCC; System Speed/Interrupts; |
| truegs | IIgs | ✅ | Tests SHR Linearization and Tech Note #201 |
| Sather 3.10B | II+ IIe IIgs | ✅ | The first cycle-counting screen demo |
| Locksmith | IIe | ✅ | Can bit-copy copy protected WOZ disks; speed test returns correct 300RPM |
| Crazy Cycles & CCII | IIe IIgs | ✅ | Correct except colorburst handling |
| XMAS Demo | IIgs | We don't handle display pixel clock shift between 14MHz/16MHz correctly |
| Woz Test Disks | IIe, IIgs | ✅ | All copy-protected woz disk images boot and run correctly |
| Alien Mind | IIgs | ✅ | Copy-protected 3.5" disk title, runs correctly |

## Arekkusu

This series of tests deserves its own section because they are so intricate and thorough.

### Switches

| Platforms | Pass/Fail | Notes |
|-|-|-|
| II+ | ✅❌ | |
| IIe | ✅❌ | |
| IIgs | ✅❌ | |

Generally speaking, GS2 is:

[ ] Doing it about half right

[ ] not correctly reflecting colorburst hold latency. We are determining colorburst per scanline based on the video mode at start of scanline. real monitors hold colorburst for some period via a PLL even after II stops sending colorburst.

[ ] not switching character widths quite right (e.g. when switching from 40 to 80, latency in the switch interacting with the video shift registers )

### CPU

| Platforms | Pass/Fail | Notes |
|-|-|-|
| IIgs | ✅ | Completely accurate |

One of the few Arekkusu tests GS2 gets completely right.

### IRQ

| Platforms | Pass/Fail | Notes |
|-|-|-|
| IIgs | ✅❌ | We partially fail QTR and SEC IRQ tests due to missing support for PAL video mode |
