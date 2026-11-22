# RamWorks

RamWorks is an Applied Engineering-style **auxiliary memory** card for the Apple //e. It replaces the 64K on the auxiliary slot with **8 MB** (128 banks of 64K). Software that knows the card can use the extra banks as a RAM disk. The picture on screen always comes from the first auxiliary bank.

It is **not** the Apple II Memory Expansion Card (“Slinky”). Slinky is a different card, in slots 1–7, with its own address registers. You can install both.

## How to enable it

### In the config editor

1. Open **+** or **Edit…** from System Select.
2. Set the platform to **Apple //e**, **Enhanced //e**, or **//e 65816**.
3. Click the **Aux** slot (slot 0 on a //e — the editor labels it Aux) and pick **RamWorks**.
4. Save and launch.

The card is always 8 MB. There is no size picker.

RamWorks is not offered on II, II+, or IIgs. Those machines do not have a //e auxiliary slot.

### In a `.gs2` file

```toml
[[cards]]
slot = 0
card = "ramworks"
```

## Using it

Boot software that expects RamWorks (or a compatible AE auxiliary card) and writes the bank number to `$C073`. Bank 0 is the ordinary 64K auxiliary memory the //e already has; banks 1–127 are the extra RAM.

GSSquared always maps **video** from bank 0, matching the real card: changing the bank does not change what you see on the composite/RGB screen. Programs that draw 80-column or double-hi-res keep the bank at 0 while they draw.

## Related

- [Creating Custom System Configs](ConfigEditor.md)
- [Writing Config Files Manually](ConfigFiles.md)
- Slinky-style expansion is a separate `mem_expansion` card in slots 1–7
