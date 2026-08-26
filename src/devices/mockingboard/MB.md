# MB Dev Notes

So the AppleWin 6522 code looks a lot like what I was doing. Calculating event timers to fire in the future, calculating what the counter's value is expected to be at a certain point, this is complex. Proceed with trying it out cycle-by-cycle.

At least, use the cycle-by-cycle to validate operation, develop tests, then we can use the tests against the more complex version.

## One-shot mode

in the one-shot mode, when the counter is set (to N), it counts down to 0, then rolls to FFFF.
Then continues to FFFE, etc. because the counter does not reset, and an IRQ is generated only once.

Both data sheets (R and WDC) imply it resets to N in one-shot mode in their figures, but that is not actually the case.

## Free-run mode

in free-run mode (T1 only) the counter is set (to N), it counts to 0, then rolls to FFFF.
it is then reset to N and repeats.

The figures that claim a T1 reset on one-shot must actually refer to free-run.

## T2 Latch - only 8 bits

the T2 latch appears to be only 8 bits wide.
because T2 is one-shot only, a typical sequence like this:
write T2L-L, write T2C-H causes T2L-L to be put into T2C-L.

$08 read: T2C-L
$08 write: T2L-L

Previous code erroneously pretends there is a hi byte latch. It did not matter in practice as we ignored the "latch hi byte" when setting the counter. But modified it to be clearer.

## Counter op: either decrement, or load

In any given cycle, a counter may either be decremented, or may be loaded. 
I.e., if we are in free-run and we have hit FFFF, we reload to the latch value, but DO NOT decrement this cycle.
If we are in one shot, and we have hit FFFF, we -do- decrement.

## When does 6502 check for IRQ status

https://gemini.google.com/app/fce7ab356521bfbd

So all the documentation I've seen says the cpu checks IRQ level at the second-to-last cycle of an instruction; or, basically, the level at the start of the last cycle. This would be easy to track by maintaining a 2-cycle (or 1?) sliding window in cpux->incr_cycle of the IRQ status and then using that as the "IRQ asserted" test instead of the current value.

IMPORTANT: The sampling is of the IRQ line AND the inhibit flag. So that's what needs to be stored into the sliding window.

in cpux->incr_cycle, we do:
    irq_sample1 = irq_sample0
    irq_sample0 = !I & IRQ

at the top of an instruction, we do:
    if irq_sample1 .. do IRQ

This is AppleWin discussing the same thing:
https://github.com/AppleWin/AppleWin/issues/718

## When does change to I inhibit bit take effect

https://gemini.google.com/app/fce7ab356521bfbd

Answer: on the second cycle of a CLI or SEI. so the IRQ check has already occurred during cycle 1. So we must make sure the change to the I flag on these instructions happens in the correct place - i.e., AFTER the phantom_read.

Hmm, BaseCPU::incr_cycles doesn't have access to the cpu struct. I either have to pass cpu to calls to incr_cycles, or, store it in the class parameters.

Instead of storing the 64-bit vector, cpu should just have a bool IRQ asserted input, like a real cpu.

I have a bunch of cross-dependency between BaseCPU and cpu_state. I should do something about that.

cpu_state->core is pretty much unused; deprecate

# Comparing Production 6522 to Test Harness 6522

W6522 - cycle-at-a-time
N6522 - callback-based

Next step is to harness the existing 6522 as-is to see how it compares to the cycle-by-cycle one.

I did that - I took the code and rationalized it as a class with the same interface as W6522, it's called N6522.

It was a full-throated hairy mess! Lots of things were wrong - counter values on reset were wrong, interrupts triggered at wrong time, no accounting for rollover to FFFF.

I got everything fixed up, though, and it seems to now be producing exactly the same results as W6522. And the code was drastically cleaned up and simplified, it even starts to make sense. LOLZ.



# Testing Approach

ok we've got a simple harness that is a sequence of 'instructions' that load a 6522 register at a certain cycle. then we just loop; display registers, tick cycle.

So now I guess we can take this, do a 2nd one, insert a cpu with an nclock with a special incr_cycle that ticks the 6522. Then I can run this  test code against it.

I implemented the 2-cycle lookback, So none of this so far has made any difference in the operation of irqtimetest. It still prints a single 0001 and then exits. and I lost 12eMHz. Need to run this test on real hardware, because it's not clear that he has done that.

## arrekusu - iigs irq test now largely failing.

main fails the QTR and 1 sec IRQ tests.
mbpercycle additionally fails the SCB and VBL tests.
I have changed behavior likely for RTI and PLP - check those routines to see if they are being modified in the correct cycles ("these should operate immediately").

## issue 94

The op insists if an IRQ is pending before CLI, and you CLI then SEI that the SEI should be interrupted. i.e. should go from CLI to IRQ.
his test program does indeed count from 1 to 100 in applewin.
is it 100 hex, not 100 decimal, so 256 interrupts.
likely what is expected here is that immediately after RTI it should trigger again.
yah yah. So it's turning the interrupt off after executing the IRQ routine x100 times.
Which means it's expecting:
interrupt immediately after CLI
and then again immediately after RTI

OK I can produce his desired results if I:
sense IRQ in 2nd to last cycle
gate with inhibit at top of instruction
this also makes arrekusu SCB and VBL tests pass again.
(the QTR and SEC are still borked so that is something else).
And I'm at 258MHz peak, so I have only lost a few MHz at this point.

The question remains: has his test program been run on actual hardware and has the behavior above been demonstrated to be correct?
(this does not resolve the Deater demo chiptunes player issues, which are likely more related to 6522 timing issues).

## mb-audit

Correctly identifies the mockingboard with two 6522 chips.
fails Test 11:04:00, Expected F1 Actual F0
this is a counter that's off by one.

Tom C isn't confident he knows the correct answers either! We need to do some science.

# New Tests

## Behavior of CLI/SEI

### IIe + Mockingboard

on a //e with Mockingboard, we can use the test program I was sent.

1. SEI
1. set up mockingboard to generate an interrupt
1. CLI and immediate SEI.

If the SEI executes, the IRQ will fire once. If the SEI does not execute, the IRQ will fire until we turn it off in the interrupt handler.

### IIgs

1. Wait until VBL clears
1. SEI
1. set up most direct IRQ vector possible.
1. enable VBL interrupt
1. spin until VBL sets
1. the VBL interrupt should now be active
1. CLI / SEI

Same test results as above.

## Behavior of RTI

Variations of the above, where we want to see if a) at least one instruction executes after an RTI before going back into IRQ handler, or b) the IRQ immediately fires after the RTI meaning unless you clear the source you will loop forever.


* Variation T1: only IRQ is sampled at penultimate cycle.

This passes both GS-IRQ test and the MB-IRQ test.

* Variation T2: both IRQ and I are sampled at penultimate cycle.

This fails both GS-IRQ test and MB-IRQ test.
However, this appears to be what perfect6502 is doing.



# Test Log

## GStest

real IIgs:
    system speed set to 1MHz
    get: 0001

## mbtest

IIe Enh/Mockingboard/65C02

IIe/Mockingboard/N6502

Xot emulator: 0001-0100

perfect6502: the SEI is executed, IRQ after that

apple2ts: interrupt never turns off, Patrick Montelo shows screencap up to 177B and still going.

real apple2e platinum: single 0001.

ApplEm (emulator): 0001 (Mike Daley)


## chiptune_dya
(tested on dual-mockingboard)
This starts playing but then fails spewing tons of 
```
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) T1 callback: 10000701
(MB_6522 2 0x00) interrupt set 1 [ T1 E1/F1 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) scheduled 10000701 at 242006557 for timer1
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
(MB_6522 2 0x00) interrupt set 0 [ T1 E1/F0 ] [ T2 E0/F0 ]:
```
not sure why it's doing this. I should print cycle counter at start of these lines.
(seems to be ok on single mockingboard)
ah, of course it's still working on dual-mb. 
I had run something before it, so maybe something was left uninitialized.

## mb-audit

Right off the bat there is a problem where the IFR is coming back E0 instead of 60.

```
DetectMegaAudioCard
	ldy		#SY6522_TIMER1H_COUNTER
	sta		(MBBase),y					; (and clears IFR.T1)

										;   T1C
										; $0006
	lda		#2					; 2cy	; $0004
	ldx		#1					; 2cy	; $0002
	cli							; 2cy	; $0000
	sta		zpTmp2				; 3cy	; $ffff
										; $0002	: real 6522 - IRQ occurs on 2nd cycle... so IRQ occurs after this 'sta zp'
										; $0001	: FPGA 6522 - IRQ occurs on 3rd cycle... so IRQ deferred until after next 'stx zp'
	stx		zpTmp2
```

So what's happening here is we are triggering interrupt after the STX, which makes mb-audit think this is a MegaAudio ("FPGA") card. This is what I was worried about. The IRQ should actually be triggered INSIDE the instruction, not after the instruction in the main loop like we do now. Before I hacked this by tweaking the MB counters, but that is not the correct approach.

Before I proceed let's re-verify the 6502/816 IRQ code is correct. It is NOT. I am still doing the whack 0001-0100 patch. I really need the real IIe HW test results back. OK, so I get the 0001 result, however, I am now failing IRQTEST A@EE and H@EE. Ah!
Right, it was because in 65816 RTI I was not setting the new P register value until after I had popped the return address, which was wrong. P needed to be set immediately after pulling from the stack so that the new value of I could be sampled.
That worked.

OK, so what we need is to put a EventTimer inside NClock, purely for use of interrupt related stuff like this if we determine it's needed. We'll do it based only on vid_cycles.


6522 Theory of operation

one-shot mode {
    in one shot mode, the counter is set.
	it decrements to 0.
	the transition from 0001 to 0000 is what triggers the interrupt.
	after this the counter continues decrementing (FFFF, FFFE, etc.)
    If the counter is set to 0, it will count 65536 cycles because the important transition is 0001 -> 0000.
	(i.e., if after decrement the counter is 0..)
    If you set counter to 0005, you will get this sequence:
	0005, 0004, 0003, 0002, 0001, 0000, FFFF, FFFE, FFFD ...
}

continuous mode {
    applies only to T1
	after decrementing to 0, the counter is reloaded from the latch.
    If the counter is set to 0, it will count 65536 cycles because the important transition is 0001 -> 0000.
    So if you set continuous mode counter to 0005, you will get this sequence:
	0005, 0004, 0003, 0002, 0001, 0000, 0005, 0004, ... 
}

each counter has a "interrupt armed" flag - no interrupt will ever be generated until after this flag is set, and it's set by writing the counter; OR only in continuous mode, when the counter is reloaded from latch.

The counters are ALWAYS counting, every single cycle. There is no state in which the counters are quiescent or static.



T1 does 'reload' on reaching zero.
but if the latch is 0, after reload, it decrements to FFFF.
this behavior is different than T2, which is always one-shot mode and just keeps counting
this is different from T1 which does the reload, and would be consistent with the WDC data sheet.
"reaches 0" means "after a decrement, the counter is 0", or, the transition from 1 to 0.
So if you program the latch with 0, it gets loaded at that point with itself, and this implies you'll get an interrupt after 65536 cycles of programming a 0.

update: Ian came up with an Apple doc about a 6523 clone core they made. It differs from the above.

Via Cell Theory of Operation

one-shot mode {
    in one shot mode, the counter is set.
	it decrements to FFFF
	the transition from 0000 to FFFF is what triggers the interrupt.
	after this the counter continues decrementing (FFFF, FFFE, etc.)
    If the counter is set to 0, it will count 1.5 cycles because the important transition is 0000 -> FFFF
	if set to FFFF, timeout occurs 65536.5 cycles later (65535 + 1.5)
	(i.e., if after decrement the counter is FFFF..)
    If you set counter to 0003, you will get this sequence:
	0003, 0002, 0001, 0000, FFFF, FFFE, FFFD ...
}

continuous mode {
    applies only to T1
	after decrementing to FFFF, the counter is reloaded from the latch.
    If the counter is set to 0, it will count 1.5 cycles because the important transition is 0000 -> FFFF
    So if you set continuous mode counter to 0003, you will get this sequence:
	0003, 0002, 0001, 0000, FFFF, 0003, 0002, 0001, 0000, FFFF, ...

	By loading the latches only, the CPU can access the timer during each countdown operation without affecting the timeout in progress.
	This will then only affect the -next- countdown period.

	The counter will start·counting down on the next C783K clock pulse following the load sequence into TlCH. The new value takes effect NEXT cycle.
}

ok, let's think about this. I already know that I -have to- be called every cycle. So I might as well just use the cycle-at-a-time design, which is significantly simpler to write and think about. ok, let's try that.

Implications:
When counter is reset in continuous mode set to FFFF, it will reset to FFFF, so it will have two cycles of FFFF in a row.
We still need to handle switching from Continuous to OneShot while the timer is armed.

uh ok there's a problem... when we're in single step mode we're calling the handler repeatedly? I could believe the debugger..
see if we're calling incr_cycle during debugger.. uh yah it's the proforma disassembler, the IRQ handler jumps to C3FA, and it tries to disassemble the first several instructions of C400, which isn't there of course. Hm. What we want is for disassembler to not be able to read any memory if it's not RAM or ROM.

ok there was a failure 11:03:00. The fix was to not decrement on the same cycle we loaded - oops regression. Ugh. So complex!

alright, so skyfox got broken again because of Clod's insistence that T1C always reloads from the T1L, even in oneshot mode. and the data sheets can certainly be interpreted that way. but that meant on power-on T1L was 0, and T1C oscillated between 0000 and FFFF, breaking auto-detect in skyfox, quarx, etc. 

So now we are initializing T1L and T1C with pseudo-random values, and I guess when the real MB gets here we can see what these things actually are on powerup.

[ ] Also, while the "mono fix" helped a lot it's not perfect. I should try a longer period than 16 samples.  

oops, test 11:14:02 was failing because of a CPU phantom read problem:
The NMOS 6502 phantom read for STA (zp),Y in write_direct_ind_x (src/cpus/base_6502.cpp:1060) used (eaddr + index) & 0xFF, which evaluated to base_lo + 2·index — not the correct base_lo + index. On STA (MBBase),Y with Y=$84 to chip-B's T1L_L, that produced a spurious phantom read at $C408 (chip-A's T2C_L), whose read side-effect silently cleared chip-A's IFR.T2 between the underflow and the ISR, dropping the T2 interrupt mb-audit 11:14:02 was expecting. Fixed to (base & 0xFF00) | (eaddr & 0xFF) (same fix applied to the page-cross phantom in read_direct_ind_x for consistency).

need to check other code in CPU for the same problem.

Uh, this isn't right:
    uint64_t get_clock_cycles() { return clock->get_cycles(); }
it should be vid_cycles. Why would it have changed this..

# Phasor Card

The system configuration editor exposes this device as **Phasor**, and new
`.gs2` files serialize it as `card = "phasor"`. Existing configurations that
use `card = "mockingboard"` remain valid as a legacy alias.

The emulated Applied Engineering Phasor powers up in mode 0, so existing
software sees an ordinary two-VIA, two-AY Mockingboard unless it deliberately
selects an extended mode. The three defined modes are:

| Mode | Name | Visible hardware |
|---:|---|---|
| 0 | Mockingboard compatible | Two mirrored VIAs, one primary AY behind each VIA, and both SSI-263 write decodes |
| 5 | Phasor native | Interleaved VIAs, all four AYs at twice the Mockingboard clock, both SSI-263 write decodes, native D7 reads, and direct speech IRQs |
| 7 | Echo+ | One fully mirrored VIA and its primary/secondary AY pair at the normal Mockingboard clock; SSI bus decode is hidden |

## Mode softswitch

The 16-byte slot softswitch is at `$C080 + slot * $10`. Thus slot 4 uses
`$C0C0-$C0CF`. Both reads and writes change the mode. Each access first clears
the accumulated mode to zero when offset bit 3 is set, then ORs offset bits
2-0 into it:

```
if X & $08: mode = 0
mode = mode | (X & $07)
```

This explains the formerly mysterious `$C0CD` access: in slot 4, offset `$D`
resets the accumulator and selects mode 5 in one operation. `$C0CF` similarly
selects Echo+ mode 7, and `$C0C8` returns the card to Mockingboard mode 0.
Accesses without bit 3 set can accumulate the selection over several bus
cycles. A card reset also restores mode 0.

## VIA and AY decode

Mode 0 retains the classic mirrored layout: VIA-A responds throughout
`$Cx00-$Cx7F`, and VIA-B throughout `$Cx80-$CxFF`. In native mode, address bit
4 selects VIA-A and address bit 7 selects VIA-B. A native write for which both
bits are set reaches both VIAs; a read ORs their results. Echo+ mode mirrors
VIA-B throughout the complete `$Cx00-$CxFF` slot page.

Native-mode reads of either VIA's T1C-L or T2C-L advance that selected timer by
one additional 1 MHz tick before returning its value, matching the Phasor
detection timing. This extra tick does not apply in mode 0 or Echo+ mode.

Each VIA has a primary and a secondary AY-3-8913-compatible PSG. Port B bits 1
and 0 remain BDIR and BC1, and bit 2 remains the active-low AY reset. In the
extended modes, bit 4 is the active-low primary-AY select and bit 3 is the
active-low secondary-AY select. Native Phasor latch behavior is deliberately
asymmetric: selecting the secondary AY for a latch cycle latches both AYs,
whereas selecting only the primary latches only it. The remembered selections
then permit primary, secondary, or simultaneous reads and writes; data from
two selected readers is ORed. The usual native Port-B sequences are:

| AY select | Latch / idle | Write / idle |
|---|---|---|
| Primary | `$0F`, `$0C` | `$0E`, `$0C` |
| Secondary | `$17`, `$14` | `$16`, `$14` |

Mode 0 ignores the extra chip-select behavior and exposes only the two primary
AYs. Mode 5 exposes all four and doubles their master clock, as a real Phasor
does. Mode 7 exposes the two AYs behind VIA-B but leaves their clock at the
normal Mockingboard rate. The two AYs behind a VIA share that VIA's existing
stereo placement.

## Dual SSI-263 sockets

Both physical speech sockets contain SSI-263AP devices; there is no SC-01 or
Votrax substitution. Speech writes are an additional decode beside the VIA
write, rather than a replacement for it. In modes 0 and 5:

| Address condition | SSI socket | Completion route in mode 0 |
|---|---|---|
| A6 = 1 | Primary | CA1 of VIA-B (`$Cx80` half) |
| A5 = 1 | Secondary | CA1 of VIA-A (`$Cx00` half) |

A2-A0 select registers 0-7, with 4-7 all aliasing FILFREQ. Because the selects
are independent, a write with both A6 and A5 set reaches both speech chips.
The coincident VIA decode still occurs according to the current card mode.

Each SSI is a mono source. GSSquared centers both sockets into the left and
right channels of the shared 48 kHz card stream with a constant-power
`1/sqrt(2)` coefficient. Software using only the usual primary socket is thus
audible in both speakers without doubling its total power. The AY banks retain
their existing stereo placement. The card mixer sums the active AY and centered
SSI sources in floating point and saturates once at the completed card output;
this avoids source-order-dependent clipping. The saturated mix then passes
through the same fixed +8 card-level warmth network used by Appletini: a
warm-band boost, one-quarter treble subtraction, and a soft knee at 20480 PCM.
The three FPGA-rate one-poles are collapsed to deterministic Q1.31 updates on
the 48 kHz card timeline. Their state survives an Apple warm reset and is
cleared by a cold/card reset.

The exact 48 kHz synthesis stream is independent of the host audio device
clock. The SDL output starts behind a 50 ms safety prefill, targets a 60 ms
queue, and trims host consumption by at most +/-0.5 percent. This prevents a
normal 1024-frame device callback or modest scheduling jitter from padding a
one-video-frame queue with audible silence; it does not duplicate, drop, or
retime SSI/AY synthesis samples.

Mode 0 slot-page reads continue to return the VIA, so SSI D7 is not directly
visible there. In mode 5 a native speech-status read requires A4 = 0, A7 = 0,
and either A6 or A5. D7 returns the selected chip's pending A/!R status and the
lower seven data bits are floating; when both speech selects are present, the
secondary socket has priority.

When enabled, a mode-0 completion drives the corresponding VIA CA1 input. The
VIA's PCR edge selection, IFR.CA1 latch, IER gating, and normal Port-A/IFR
acknowledgement rules then apply. In native mode the same completion asserts a
direct card IRQ rather than passing through the VIA. A pending D7 request is
level-routed when the mode changes: entering mode 0 presents it to CA1,
entering mode 5 asserts the direct IRQ, and leaving native mode removes that
direct assertion. Thus switching modes neither loses nor duplicates a pending
speech response.

## SSI-263 bus timing and reset behavior

SSI response and duration timing runs from the effective approximately 1 MHz
XCK clock, separately from the fixed 20 kHz formant-control and coefficient
clock and the shared 48 kHz card audio stream. With duration field `D` and RATE
high nibble `R`, one complete phone cycle is exactly:

```
(4 - D) * 4096 * (16 - R) effective XCK edges
```

The 48 kHz samples are rendered incrementally on that emulated XCK timeline,
so a phone, closure, RATE, or reset transition inside a video frame affects
only subsequent audio instead of being applied retroactively to the frame.

In response mode 1, A/!R occurs once per `4096 * (16 - R)`-edge frame while
the phone itself has three frames. Modes 2 and 3 respond at the complete phone
boundary. A zero duration/mode field disables the external A/!R route but
retains the previously selected response/inflection mode; D7 still records
the response at that retained mode's boundary. A completed phone repeats from
its first timing phase until software writes another phone or powers the chip
down.

Writes to DURPHON, INFLECT, or RATEINF acknowledge D7. DURPHON starts the newly
latched phone, but INFLECT and RATEINF only clear the request; they do not
restart the tract, audio, or duration phase. A RATE change takes effect on the
next internal 1/16-frame slot, never retroactively on the slot already in
progress. RATE controls public duration and response timing only; it does not
speed up articulation.

CTTRAMP bit 7 powers the output down, mutes it, clears the visible request, and
suppresses new A/!R assertions. Lowering the bit starts the latched DURPHON
again with a full response interval. The first transitioned-inflection phone
seeds its pitch from the first live 12-bit target instead of sweeping up from
an artificial zero; subsequent transitioned phones retain the running upper
pitch target.

A cold reset sets DURPHON=`$C0`, INFLECT=`$00`, RATEINF=`$00`,
CTTRAMP=`$80`, and FILFREQ=`$00`, clears D7, and clears synthesis history. An
Apple reset is a warm reset for the SSI-263AP: it powers speech down and
releases D7/IRQ while preserving DURPHON, INFLECT, RATEINF, FILFREQ, and the
transitioned-inflection seed/state.

FILFREQ remains a readable/writable register latch, including the register 4-7
aliases, and follows the reset-retention rules above. To match the Appletini
backend, however, it is acoustically unused: it does not retune the fixed 20 kHz
formant coefficient model, and no value (including `$FF`) is a mute command.
IIR history is invalidated at a phone boundary, matching the Appletini
validity-mask behavior and avoiding stale-coefficient crackle without resetting
the oscillator or interpolation state.

## Speech synthesis fidelity

Speech is synthesized in real time from a glottal source and a 15-bit LFSR
noise source. It never plays recorded phoneme samples. The exact active
512-byte native SSI-263A/SC-02 ROM table covers all 64 SSI phoneme and
allophone codes (SHA-256
`101d129a5f104e6190f2eca518bbf9ef65bf4ff92684d29eba56d9641aa02b0a`).
These bytes are control parameters, not audio. Ordinary phones use the native
targets directly; the capture-proven HV/HVC/HFC/HN hold phones instead freeze
the instantaneous interpolated tract state so an established articulation is
not lost.

The renderer is intentionally a hybrid model: native SSI parameters drive the
retained SC-01A-compatible F1, F2 voice/noise, F3, F4, closure, and output
topology derived from Olivier Galibert's `vsim`/MAME work. GSSquared implements
the Appletini signal path at 48 kHz with Q15 filter coefficients, saturating
24-bit intermediate stages, and signed 16-bit output. Its output stage mirrors
Appletini's soft limiter (a knee at +/-8192 with 4:1 compression beyond it) and
limits adjacent output changes to 3000 counts per sample. The result is then
converted to the card mixer's floating-point domain for constant-power stereo
placement and the single final card-output saturation described above.

This remains neither a transistor-level SC-02 model nor a claim of equivalence
to every analog SSI-263. Native selector-4/control routing that has no
counterpart in the retained tract remains outside this hybrid model.
