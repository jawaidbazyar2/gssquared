# RamWorks

Implementation spec for an Applied Engineering RamWorks-style auxiliary memory card on the Apple IIe models. Not implemented yet.

RamWorks is not the Apple II Memory Expansion Card ("Slinky"). Slinky is a slotted RAM disk with its own address registers. RamWorks replaces the 64K on the IIe auxiliary slot with N banks of 64K and leaves the motherboard's soft switches in charge of *when* the CPU sees auxiliary memory.

## Model

N auxiliary banks, numbered **AUX 0 .. AUX N-1**.

- **AUX 0** is the stock IIe auxiliary 64K (the extended 80-column card).
- **AUX 1 .. AUX N-1** are the additional banks.
- Each bank is a full 64K, including the `$C000–$CFFF` hole where language-card bank 1 is stored.
- The motherboard still decides main versus auxiliary: `RAMRD`, `RAMWRT`, `ALTZP`, `80STORE`, `PAGE2`, `HIRES`.
- The card only decides *which* 64K auxiliary bank those accesses hit.
- **Video always reads AUX 0.** The scanner does not follow the bank register.

A machine with no RamWorks card is N = 1. That is the IIe we already emulate. Do not change that allocation, those pointers, or those page mappings.

Supported sizes are powers of two, from 2 banks upward:

| `banks` | Auxiliary RAM | Bank numbers |
|---------|---------------|--------------|
| 2 | 128K | AUX 0–1 |
| 4 | 256K | AUX 0–3 |
| 8 | 512K | AUX 0–7 |
| 16 | 1MB | AUX 0–15 |
| 32 | 2MB | AUX 0–31 |
| 64 | 4MB | AUX 0–63 |
| 128 | 8MB | AUX 0–127 |

A written bank number is masked with `N - 1`. Non-power-of-two sizes (the 3MB / 48-bank RamWorks III population) are out of scope.

## Where it sits

Real RamWorks plugs into the auxiliary slot. The IIe has no slot 0; the language card occupies slot 0 only on the II and II+. Use **slot 0 as the config stand-in**, on these platforms only:

- Apple IIe
- Apple IIe Enhanced
- Apple IIe Enhanced + 65816

Not the II, II+, or IIgs. The IIgs `computer->mmu` pointer is the Mega II, which is also an `MMU_IIe`. Offering the card there would grow the wrong memory.

The card has no slot ROM and no `$C800` firmware. Its only bus presence is the bank register.

```toml
[[cards]]
slot = 0
card = "ramworks"
banks = 16
```

`banks` counts auxiliary banks and **includes AUX 0**. It is required, and it must be one of the sizes in the table above. The config editor offers the card in slot 0 on the IIe platforms, with a size control that writes `banks`.

Slinky stays a separate slot 1–7 card and can be installed next to RamWorks.

## Physical layout

`MMU_IIe` keeps the 128K block it allocates today. The card does not reallocate it.

```
main_ram + 0x00000   motherboard main 64K
main_ram + 0x10000   AUX 0
aux_extra + 0x00000  AUX 1
aux_extra + 0x10000  AUX 2
...
```

`aux_extra` is `(N - 1) * 64K`, allocated by the card at startup. `get_memory_base()` / `get_memory_size()` continue to describe the 128K block. Video, 80-column text, the debug video view, and anything else that already adds `0x10000` keeps meaning AUX 0.

Fill `aux_extra` with the same power-on pattern `MMU_IIe` writes into AUX 0 (`CC CC 00 00` repeating).

## The one pointer

`MMU_IIe` holds the CPU's current auxiliary bank:

```
uint8_t *cpu_aux;   // starts as main_ram + 0x10000
```

`iiememory_compose_map` and `bsr_map_memory` use `cpu_aux` as the base of every region they currently build with `+ 0x10000`. Main regions stay at `main_ram + page`. With no card, `cpu_aux` is set in the `MMU_IIe` constructor and never moves, so the composed map is the one we have now.

`MMU::read` and `MMU::write` stay direct `read_p` / `write_p` loads. The bank register does not belong on that path.

A bank write updates `cpu_aux`, then forces those two composers to republish. The composers skip a region when the corresponding `m_*` flag did not change, so a bank change with the same `RAMRD` / `ALTZP` / `80STORE` state would otherwise leave the page table pointing at the previous bank. Language-card bank 1 versus bank 2, read-enable, and write-enable stay as they are. Only the 64K those pages point into changes.

Regions that follow `cpu_aux` when the motherboard has selected auxiliary memory:

| Switch | CPU pages | Which bank |
|--------|-----------|------------|
| `ALTZP` | `$00–$01` (zero page and stack), and the language-card window `$D0–$FF` | selected aux bank. LC bank 1 still lives at `$C000` inside that bank |
| `RAMRD` / `RAMWRT` | `$02–$03`, `$08–$1F`, `$40–$BF` | selected aux bank |
| `80STORE` + `PAGE2` | text `$04–$07` | selected aux bank |
| `80STORE` + `PAGE2` + `HIRES` | text `$04–$07` and hires `$20–$3F` | selected aux bank |

Read and write maps still differ (`RAMRD` versus `RAMWRT`). Both auxiliary sides are the same selected bank.

`80STORE` does not pin text or hires to AUX 0. With the register nonzero, a CPU write to `$400` under `80STORE`+`PAGE2` lands in the selected bank, and the display still refreshes from AUX 0. Software that draws 80-column or double-hires keeps the register at 0 while it draws. Software that uses the extra banks as a RAM disk switches the register and expects the screen to stay on AUX 0.

## Video

Leave these on `main_ram + 0x10000`:

- `VideoScannerIIe` and `VideoScannerIIePAL` (`ram[address + 0x10000]`)
- `text_page::kAuxBankOffset`
- the debug video view's auxiliary page

The scanner caches `get_memory_base()` at construction. That is safe because this design never moves `main_ram`.

## Registers

| Address | Access | Effect |
|---------|--------|--------|
| `$C073` | write | Select the auxiliary bank. Value is masked with `N - 1`. |
| `$C071` | read | Return the current bank number. Bits above the mask read as 0. |

Any access in `$C070–$C07F` also strobes the paddle timers, as on a stock IIe. The game controller already owns that range.

`$C0xx` has two handler slots, and both run. Built-in devices power on before slot cards, so the paddle strobe is slot 0 and the card takes slot 1.

- **Write `$C073`:** both handlers run. The card stores the bank. The paddle strobe still fires.
- **Read `$C071`:** the two read results are OR'd, and the paddle handler returns the floating bus. The bank number has to be the value the CPU reads, not the bank OR'd into the floating bus. The strobe side effect still happens.

Reset clears the bank to 0, points `cpu_aux` back at AUX 0, and republishes. That belongs with the rest of `reset_iiememory`, which already returns the IIe to main RAM.

## Startup

Order today, which this design relies on:

1. `MMU_IIe` allocates 128K. Constructor sets `cpu_aux = main_ram + 0x10000`.
2. Built-in `power_on`. Display caches the RAM base. IIe memory caches it and composes the map against `cpu_aux`. The game controller hooks `$C070–$C07F`.
3. Slot `power_on`. RamWorks calls `MMU_IIe::install_ramworks(N)`, which allocates `aux_extra` and leaves `main_ram` in place, then registers `$C071` and `$C073`.
4. CPU reset. The bank is already 0; reset makes that explicit.

No guest code has run at step 3, and nothing that cached `main_ram` needs to be told about the extra banks.

## Save and debug

A snapshot of a machine with the card installed stores the bank index and the `aux_extra` bytes along with the 128K block. The 128K image alone is main plus AUX 0.

CPU reads and writes through the MMU see the selected bank. A raw dump of `get_memory_base()` shows main and AUX 0 only. A debugger view of AUX 1 and up is a follow-on; it is not required for the card to function.

## Out of scope

- Moving or resizing the 128K `main_ram` block.
- A bank test inside `MMU::read` / `MMU::write`.
- Pointing the video scanner at the selected bank.
- Forcing `80STORE` text/hires windows to stay on AUX 0.
- IIgs / Mega II.
- Slot ROM or a `$C800` driver.
- RamWorks III sizes that are not a power of two.
- The 3MB expander's partial-decode aliasing.
