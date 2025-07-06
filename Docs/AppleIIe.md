# Apple IIe Functionality Checklist

Here is the Apple IIe checklist:

[x] 128K memory management  
[x] 80 column text  
[x] dlgr  
[x] dhgr  
[x] VBL  
[x] Alternate character set (I implemented the switch but don't act on it yet)  
[x] display must read video memory from absolute memory, not from page table
[x] have "default to rom" concept in mmu. (mostly resolved by moving switches into mmu)  
[x] modifier keys mapped to open-apple and closed-apple  
[x] up and down arrow keys  
[x] AKD flag  
[x] read last key press from anywhere in C000-C01F  
[canx] implement C040 strobe - related to game I/O  it's a strobe output unused. Doesn't make sense in emulator context. Yet.
[x] C071-7F same as C070 (missing in apple ii+ impl also)  
[ ] on control-oa-reset don't clear locations like on ii+  
[x] self-test fails mmu flag: e4:D

## Notes

### read last key press from anywhere in c000-c01f

we are missing: c011, c012, c015, c01e, c01f.

This is ugly. This should be done using the messaging bus concept, instead of creating coupling between modules.

OK this has been done, through use of the MessageBus. I'm still not entirely sure I like the approach, but, it works so far.

### AKD

this is "any key down". The current implementation checking GetKeyboardState isn't great (I have to scan an array that could be quite big every time). Glider works ok with it. Sorta. "AKD" includes shift, so if I hit an arrow and let go, but then hit shift and hold that down, it moves because it's reusing the previous key down value. Yeah, the array is of 512 keys and we're iterating that every time. Icky.

So let's monitor key up and key down and track the difference, and if "keysdown>0" then AKD is true. We need to disregard just modifiers for these purposes.


### VBL

Glider is sometimes becoming invisible, then coming back. Seems clearly to be a vbl issue. I currently allocate 4,550 cycles for vbl. They could be de-synchronized compared to our actual screen updates. hmm. main loop is 17,008 cycles. that's 60hz. Apple II video is actually 59.9x Hz. well let's try vbl at 17,008. starts out invisible but so far has been good. I think we need to start the count of 4,550 cycles at the start of a frame. This is in display, so the update_apple2_display thing can set the vbl cycle start.

The issue is we're never going to divide by exactly how many cycles went by, using a fixed count because the actual number of cycles in the cpu execution loop is going to vary depending on the exact instructions (some instructions may run long).

ok, this is working better so far off the bat. The glider showed up first thing and now I'll sit here and watch and see what happens. Lots better!!

### Read last key press from anywhere in C000-C01F

most of these are in iie memory but not all of them; they need to call iiekbd to get the "last key value" and return that. This is all IIe stuff, so, we can have these modules be dependent.

### Modifier Keys / OA-CA

This is fine, per discussion in DevelopLog. we might need to check platform windows/linux and switch these to ALT.

### Self-Test fails: 80 store switch won't change

Getting an MMU E4:D code. That is "80store switch won't change". That's not right..

actually, code trace seems to indicate the issue is: c00b, then c00a, then read c017. this is -not- 80store. This is SLOTC3ROM.
Ahh, slotc3rom wasn't working correctly. The message was wrong! it's in the mmu but iiememory was overwriting it.
Now the self-test passes. I wonder if some recent things that weren't working will work now..


### Character Sets

And just general character set.

#### normal character set
| screen code | mode | characters | Pos in Char Rom |
|-|-|-|-|
| $00..$1F | Inverse | Uppercase Letters | 00 - 1F |
| $20..$3F | Inverse | Symbols/Numbers | 20 - 3F |
| $40..$5F | Flashing | Uppercase Letters | 00 - 1F* |
| $60..$7F | Flashing | Symbols/Numbers | 20 - 3F* |
| $80..$9F | Normal | Uppercase Letters | 80 - 9F |
| $A0..$BF | Normal | Symbols/Numbers | A0 - BF |
| $C0..$DF | Normal | Uppercase Letters | C0 - DF |
| $E0..$FF | Normal | Symbols/Numbers | E0 - DF |

#### alternate character set
| screen code | mode | characters | Pos in Char Rom |
|-|-|-|-|
| $00..$1F | Inverse | Uppercase Letters | 00 - 1F |
| $20..$3F | Inverse | Symbols/Numbers | 20 - 3F |
| $40..$5F | Inverse | Uppercase Letters (tb mousetext)| 40-5F |
| $60..$7F | Inverse | Lowercase letters | 60-7F |
| $80..$9F | Normal | Uppercase Letters | 80 - 9F |
| $A0..$BF | Normal | Symbols/Numbers | A0 - BF |
| $C0..$DF | Normal | Uppercase Letters | C0 - DF |
| $E0..$FF | Normal | Symbols/Numbers | E0 - DF |

So a "character set" is:

* index
* mode (normal, inverse, flash)
* map to character rom position

For IIPlus: build this table 1:1 to chars in rom except populate mode=flash based on hi bit.
For IIe: build 2 tables; 1st per "norm" above, but override 40-7F to point to 0-3F and set mode flash; 2nd table, per "alt" above.

ok, so you CAN haz flash in 80 cols. And, my "detect a flashing character" routine was just modified to support looking at the 80-col page. However, it's ugly and will also match and cause update for non-flashing inverse characters when alt char set is enabled. oh, we can do is_flash against the char rom, that will take active char set into account. Even better, if altcharset is 1 we can probably just exit and not do the update_flash routine at all.

# Enhanced Apple IIe Functionality Checklist

[ ] different video rom  
[ ] different system rom  
[ ] 65c02  
[ ] different select tile, overlay the "enhanced" sticker on top of it.  
