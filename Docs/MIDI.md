# MIDI

Idea: emulate Passport MIDI Card

Include FluidSynth in the build to do the synthesis and MIDI command stuff.

It will need to run as a separate thread, because we'll need to feed commands to FluidSynth at specific timestamps.
So these would be queued and the FluidSynth thread will dequeue to process a synth change at a specific realtime.
It has an event queue built in, see: "fluid_sequencer_send_at"

This could then also be a great thing to have on a real card.

So you'd have a card that would do:

MIDI Synthesis
Mockingboard
ECHO II (or Echo Plus which was basically speed + mockingboard). Uses TMS5220 chip (implemented in MAME)

Any other sound cards for the Apple II?

Too bad there isn't likely to be that much software for it. Well there will be some things. There's Ultima V. PassPort's own software.

This is an interesting library I saw Ian working with:

https://github.com/munt/munt
