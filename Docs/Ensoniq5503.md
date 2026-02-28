# Ensoniq DOC 5503

This is the "S" in the Apple IIgs

Brief summary:

* 64K RAM for 
* 224 + n registers to control 32 'oscillators'

The oscillators are basically 24 bit counters, which increment by a 16-bit "frequency" value each cycle.
Any given cycle, the DOC only increments one of these counters (done in sequence).

Oscillators can be active or inactive.

Oscillators can be paired in different way:
* waveform + envelope
* tag-team (payback alternating back and forth between the two oscillators)
* etc.

Oscillators are mapped to channels / voices, and to one of 8 analog output channels. These are time division multiplexed using CA0-CA2 pins. The apple iigs built-in sound squashed this into one mono channel; there are stereo cards and 4-ch quadrophonic cards available. Nothiing with 8. So effectively, we will target two channel and likely divide using CA0 (lowest bit) of channel assignment.

The other thing here is that we might be modifying sound RAM during playback. This implies a per-cycle generation of output audio.

With a 7.1mhz input clock, divided by 8 internally, this implies a 887,500hz maximum rate. However the IIgs has output filtering around 26KHz. And let's be honest, probably nobody is going to run it that fast.

We don't have to simulate the 5503 bug where output value drops to 0 in swap mode when switching from higher-numbered osc to lower. This might help actually.

So the architecture is going to be like this:

1. from cpu, call audio_cycle
1. this will call 5503cycle as many times as is necessary to "catch up" to current cpu time interval.
1. each call to 5503 cycle will iterate one oscillator, if active.
1. osc output stored into temp buffer
1. at Frame time, data pulled from temp buffer and sent into the SDL audio buffer.

"time interval" is not a cycle count, it's a ns count. 

TBH this might be a good application for multithreading. But I am still concerned about syncing between threads and getting put into sleep waiting for sync between threads.

An alternate approach above might be to stream all modifications into a buffer, that is reconstructed at frame time. i.e. we just event log both register changes and data ram changes, and apply in a frame handler. That buffer might need to be quite large but so what? This decouples the clock timing in different parts of the system (cpu vs video vs audio). 

Getting a lot of mileage out of this concept, eh?

So, the registers are in page 0. So events are:
* event time
* address
* value



## References

https://gswv.apple2.org.za/USA2WUG/A2.LOST.N.FOUND.CLASSICS/SOUND.GS.5503.ensoniq.txt
https://mirrors.apple2.org.za/ftp.apple.asimov.net/documentation/hardware/schematics/Apple%20IIgs%20schematic.pdf
https://www.brutaldeluxe.fr/documentation/cortland/v4_13_EnsoniqDOC.pdf
https://web.archive.org/web/20221112230915if_/http://archive.6502.org/datasheets/ensoniq_5503_digital_oscillator_chip.pdf

# Testing

ok, the ROM self test is failing. 

it's failing in a way that it basically is incrementing the address twice when it expects to once. From the Cortland docs:

Read operation
The Sound RAM read operation is the same as the write operation with one
exception-reading from the data register lags by one read cycle. For example, if you want
to read 10 bytes from the sound RAM, select the RAM by setting the control register bit
and enabling auto incrementing. Then set the address pointer to the starting address and
read the data register eleven times, discarding the fIrst byte read.

