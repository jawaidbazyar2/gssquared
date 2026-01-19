# Apple IIgs Diagnostics 2.0

This is a log of attempting to pass the diagnostics on this disk.

## Press Space to Continue

at the start, "Press space". It's failing already because C010 is always returning EE.

It's doing a tight loop:
```
repeat BIT C010
       BPL repeat
```

The hi bit of this is "any key down". if I press a key (say, 'D'), C000 gets a D with hi bit set, and C010 has the same.
then you read C010, and C000 hi bit => 0. As long as you hold D, C010 hi bit is set. Once D is released, C010 hi bit goes => 0.
ok. We can detect AKD as follows:
track a counter, "keysdown". Disregard modifier keys. Any other key, a key-down adds 1 to this counter; a key-up subtracts 1 from this counter.
if the counter is non-zero, then "any key down".

OK, that's fixed.

## Keyboard test

It's not detecting caps lock down. or control etc for that matter.
So for stuff to work right and make sense, the value of the mods must be latched with and tied to the key value. That's in the docs somewhere.
But perhaps when there is nothing in the latch, we should return currmod.
So, if !(c000&80) return currmod.

OK, I got this going along with the keypad.

Mouse!

Cortland docs are backwards from IIgs hw ref in respect to "mouse x available" bit in c027 . 0 means "x coord is avail", 1 means "y coord is avail".
my X is backwards. The button sense is backwards.

OK. So mouse motion down seems scaled reasonably.
However, when I move up a fraction, it jumps to the top of the screen. i.e., it's whackadoodle. I think the field is NOT 2's complement.
ok, I have the polarity right now.. but the negative motion still is definitely wrong..
oh, it IS two's complement.