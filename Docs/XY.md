# XH / YH clamp: original vs current edits

Working note. Compares committed 65816 index-high behavior with the pending `clamp_index_hi` work. Both are wrong. Zany Golf’s playfield blit starts working under the edits; Wolfenstein 3D then dies on startup.

## Hardware contract

WDC: if X=1, index registers are 8-bit and **the high byte is forced to 0**. Entering emulation (E=1) forces M=1, X=1, SH=`$01`, and therefore XH=YH=`$00`. B (A high) is **not** truncated when M=1.

Bruce Clark’s operational statement (and the usual Super Famicom tables) is an **edge**, not a level:

- When X **is being set** (0→1) — SEP, PLP, RTI, or E 0→1 via XCE — XH and YH become `$00`.
- Those zeros **persist** after X returns to 0. `SEP #$10` / `REP #$10` does not restore the old high bytes.
- While X=1 (or E=1), a write to XH/YH has no effect. 8-bit LDX/PLX/TAX only touch XL.
- M is different: B survives `SEP #$20`.

So the machine keeps one invariant: **if X=1 then (XH,YH)=(0,0)**, established on the 0→1 edge and preserved because 8-bit ops cannot write the highs.

## How this emulator actually holds X/Y

Five cores, switched at the **start** of the next instruction (`CPU65816::update_current_core_if_needed`):

| E | M | X | Core | `_X` / `_Y` bind to |
|---|---|---|---|---|
| 1 | (forced 1) | (forced 1) | emulation | `x_lo` / `y_lo` |
| 0 | 0/1 | 1 | native *8 | `x_lo` / `y_lo` |
| 0 | 0/1 | 0 | native *16 | `x` / `y` (16-bit) |

The union (`x_lo`/`x_hi` over `x`) means an 8-bit core **never writes** `x_hi`/`y_hi`. High bytes are a shadow that only becomes architecturally visible when a 16-bit-index core is selected (X returns to 0), or when something uses the full 16-bit register (TXS is the sharp example: SH gets XH).

Core switch is one instruction late: the opcode that changes X still runs on the **old** core. That is fine for SEP/PLP/RTI/XCE (they do not store into X/Y as data). It is why a clamp has to live **inside** those opcodes, not only in the dispatcher.

## Original (committed) behavior

No helper. No clamp in `cpu_65816.cpp`. Zeros are hand-written at three sites:

| Event | Zeros XH/YH? |
|---|---|
| Reset | yes (explicit stores, then E=M=X=1) |
| SEP, native (`E=0`) | yes, if `_X==1` after `P \|= n` |
| SEP, emulation | no (forces M/X bits only) |
| XCE, E becomes 1 | yes, plus SH=`$01`, M=X=1 |
| XCE, E becomes 0 | no |
| Native PLP | **no** (TODO: “x/m mode switch check”) |
| Emulation PLP | no (forces M=X=1 only) |
| Native RTI | **no** |
| Emulation RTI | no (keeps B/unused bits) |
| REP | **no** (TODO: “register width change”) |
| CLC/SEC/CLI/… | no (correct; they do not write X) |

So the original only implements the 0→1 wipe for **SEP in native mode** and **enter emulation**. PLP and RTI can set X and leave a **nonzero shadow** in `x_hi`/`y_hi`. 8-bit cores ignore it. The next `REP #$10` (or any X 1→0) selects a 16-bit core and the shadow is suddenly live.

## Current edits

`clamp_index_hi` after every architectural write of P or E:

```text
if (has_65816 && (_X || E)) { x_hi = y_hi = 0; }
```

Call sites: reset, native PLP, emulation PLP, RTI (both), REP, SEP, XCE (both directions). Dispatcher clamp was tried and removed; it is not in the hot path now.

This is a **level** rule: “after this P/E write, if X or E is set, destroy the highs.” It is not “when X is being set.”

| Event | Original | Edited |
|---|---|---|
| Reset | wipe | wipe (`_X \|\| E`) |
| SEP native, X becomes 1 | wipe | wipe |
| SEP native, X already 1 | wipe | wipe |
| SEP / REP in E | no wipe | wipe (`E`) |
| XCE E 0→1 | wipe | wipe |
| XCE E 1→0 (X stays 1) | no wipe | wipe (`_X`) |
| Native PLP, pulled X=1 (0→1 or 1→1) | **keep shadow** | **wipe** |
| Native PLP, pulled X=0 | keep (correct) | keep |
| Native RTI, pulled X=1 | **keep shadow** | **wipe** |
| Native RTI, pulled X=0 | keep (correct) | keep |
| REP, X stays 1 | keep shadow | wipe |
| REP, X becomes 0 | keep (persist) | keep |

The only **new** destructions versus original are: native PLP/RTI with X=1, REP while X remains 1, any P write while E=1, and XCE leaving emulation.

## Why the original is wrong (Zany Golf)

Zany’s mask blit (`$01/0C64` → `$01/0D1B`) was computing source longs from LocInfo at `$0C28` with garbage in the 16-bit Y / rows fields (observed bank `$83`, etc.). HUD/music were fine; the playfield path is 16-bit-index arithmetic.

That matches a **missing 0→1 wipe on PLP/RTI**:

1. X=0, Y (or X) holds a real 16-bit value; `y_hi` is nonzero.
2. PLP or RTI pulls a P with X=1 (PHP from an 8-bit or emulation context, toolbox wrapper, IRQ/RTI). No SEP on this path, so original does not zero `y_hi`.
3. 8-bit core runs; stores only `y_lo`. Shadow `y_hi` sits untouched.
4. Later `REP #$10` (or equivalent) brings a 16-bit core back. Addressing uses the **full** Y, including the stale high byte.
5. Blit walks off into the wrong bank and never takes the `$01/0DBD` exit.

SEP-only clamping cannot see this path. The TODOs on PLP and REP are exactly this hole. Zany starts drawing once those P restores also wipe — which is the part of the edit that is aimed at the real rule.

## Why the edits are wrong (Wolf3D)

Wolf3D already booted and played on the original CPU. The edits make startup crash. The new wipes are the only X/Y change.

The helper does not ask “did X go 0→1?”. It asks “is X or E set **now**?” after **any** P or E store. That over-fires in ways the datasheet does not.

### 1. Level-triggered wipe on 1→1

PHP/PLP and IRQ/RTI around code that is **already** X=1 re-enter `clamp_index_hi` even though X did not change. On hardware that is a no-op **only if** XH/YH are already 0.

In this emulator they often are not. Original left a shadow from the last 16-bit load. 8-bit cores do not clear it. A later PLP/RTI/REP that **lands** with X=1 now destroys that shadow.

Wolf3D / GS/OS / toolbox startup is exactly that traffic: nested PHP/PLP, tool calls, ADB IRQs, SEP/REP. If init does the common (hardware-illegal) pattern:

```text
PHP              ; stacked P has X=1  (8-bit or emulation PHP)
REP #$30
LDX #ptr         ; 16-bit pointer in X (and/or Y)
LDY #ptr
; … use them …
PLP              ; X bit comes back 1
```

then:

| | After PLP | After a later `REP #$10` |
|---|---|---|
| Hardware | XH=YH=0 | `$00xl` / `$00yl` |
| Original | shadow still `ptr` | **full `ptr` again** |
| Edited | highs forced 0 | `$00xl` / `$00yl` |

Original Wolf3D init can keep using the 16-bit pointer after a PLP that “only meant to restore C/I/D.” Edited (and hardware) truncate. If this build of the game, on this ROM path, never reloads X/Y after that PLP, startup dies (bad bank, bad tool dispatch, or TXS with XH=0 → stack at `$00xx` instead of `$01xx`).

That is not “more correct SEP.” It is applying a **destroy** to every P restore whose **result** has X=1, including restores that original treated as flag-only.

### 2. `|| E` is the wrong predicate

E=1 already forces X=1 on a correct XCE/PLP/REP/SEP path. Testing E as an independent reason to wipe means: **any** P write in emulation zeros XH/YH even if `_X` is 0.

In emulation, P bit 4 is **B**, not X. A pull can make `_X` read as 0 until the e-mode PLP stanza forces it back. The helper still wipes because `E` is 1. That is not “X is being set.” It is “we touched P while emulating.”

XCE to native also wipes because X stays 1 (`E` is now 0, `_X` is 1). Highs should already be 0 if enter-E was correct; if they are not, this is another silent 1→1 destroy.

### 3. Still not an invariant

The helper only runs on P/E writes. It does not make “write XH while X=1 has no effect” true as a **register** rule (8-bit cores already approximate that by binding `_X` to `x_lo`). It also does not remember the previous X, so it cannot implement the documented edge.

So the edit is not a complete 65816 model. It is a broader hammer on the same three bytes original already sometimes cleared.

## Same hole, opposite games

| | Shadow XH/YH after PLP/RTI with X=1 | Zany blit | Wolf3D startup |
|---|---|---|---|
| Original | **kept** | uses stale 16-bit Y → black / infinite blit | uses leftover highs as live pointers → boots |
| Edited | **always cleared** | highs 0 → LocInfo math sane | pointers truncated → crash |
| Hardware | cleared on **0→1 only**; stay 0 while X=1 | depends on the game’s actual P sequence | depends on the game’s actual P sequence |

Both games are sensitive to the **same** fact: this core design stores a 16-bit X/Y all the time and only *sometimes* forgets the high half. Original forgets too rarely. Edited forgets whenever P or E is written and X or E is set.

Zany’s failure mode is “stale high byte is garbage.” Wolf3D’s failure mode is “stale high byte was the value the program still wanted.” Treating those as one bug with one wipe is why one title flips from broken to working and the other the reverse.

## What neither version does

A correct implementation (not done here) is:

1. On **X 0→1** only (SEP/PLP/RTI, and E 0→1 which forces X), write XH=YH=0 once.
2. Do **not** wipe on P writes that leave X=0 (16-bit values must survive PHP/PLP that restore X=0 — both versions already get this right).
3. Do **not** wipe on 1→1 unless you are enforcing a broken shadow; on hardware there is no shadow to kill.
4. Do **not** use `E` as a substitute for X. After the E=1 side effects (`M=X=1`, SH=`$01`), the X edge is enough.
5. Keep 8-bit cores bound to `x_lo`/`y_lo` so XH/YH cannot be written while X=1.
6. Leave M/B alone.

Original fails (1) for PLP/RTI. Edited does (1) but also (the opposite of) (3) and (4). That is why both are incorrect, and why the two games disagree about which incorrect model to ship.
