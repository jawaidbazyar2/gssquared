# IWM - Integrated Woz Machine

The IWM was developed as a more flexible and compact form of Woz's Disk II disk controller.

it supports two different main timing inputs, 7MHz (used in apple IIs) and 8MHz (used in Macs).
And it supports two different speeds (densities), a 4uS cell (disk ii) and a 2uS cell (3.5 drive).

It has 4 internal registers and also the ability to communicate status and commands to/from the (dumb) 3.5 drive. And to talk to the smartport. (This last bit, I am unclear how it specifically works).

And it does all this using the same 16-byte I/O locations as a standard Disk II, while having a default mode that is exactly Disk II compatible. Crazy piece of work, this!

Original Apple Spec
https://www.brutaldeluxe.fr/documentation/iwm/apple2_IWM_Spec_Rev19_1982.pdf


Nick Parker's 3.5-focused treatment - but should give a good idea of how both operate (since 5.25 is just 'default mode')
https://www.applefritter.com/files/2025/03/02/IWM-Controlling%20the%203.5%20Drive%20Hardware%20on%20the%20Apple%20IIGS_alt.pdf


## Refactoring Disk II

see DiskII.md for details.

Initially, just implement 5.25 support on IWM. Then we'll start to add in 3.5 and see if we need a different abstraction layer.

ok, I found the splice point - we need to pass the register read/writes down to the Floppy layer also. I pass them all, it can ignore the controller-level ones.

This is quite the layer upon layer of classes, but these are all header implementations and they are honestly not that complex, so the compiler should be able to optimize pretty well.

I'm now at the point where I need something to wire in the hooks to OSD/mount. I guess I can manually mount a floppy image for testing before I do that.

We need to reference the DISKREG register to control whether we're accessing 5.25, or 3.5. This will control which drives we call in the code. Drives 0-1 are 5.25; 2-3 are 3.5. We generate the index by: C031[6]. 0=5.25, 1=3.5. So it's that bit, shifted left one, plus the drive select switch.

ok, that's done. Now on to ...

## AppleDrive 3.5 Support

Key things here. Probably first thing to start with is, implement the interface to the 3.5 drive registers. There are 16 of them. They are selected by using CA0-CA2, plus SEL. You can read whether head is at track 0 or not; choose the disk head; read number of sides; detect if motor is on; detect motor speed; if a drive is even connected; if a diskette is inserted; if write protected; etc.

There are also 7 control functions. Those registers are selected the same way, but then triggered by toggling LSTRB. The control functions move the disk head, set head direction, turn the motor on and off, eject the disk.

The low level disk read/write stuff is mostly the same, though the 3.5 has variable sized tracks we need to account for. When we move the head to a different sized track we need to interpolate the head position (i.e. the index into the buffer) appropriately.




## Notes on simulating delayed head movement

We can use a frame time handler to simulate head movement latency. In the 5.25 world, the driver code is responsible for moving the head, then doing a busy wait for an appropriate length of time to let the head settle down. On the 3.5, that is handled in the drive and the OS is looking for a "head movement is done".

So we can tick that state machine reasonably accurately during frame time. Basically, we want to wait for a whole frame to have elapsed. This will probably be ok. If it ends up being too slow, we can use a 14M cycle checkpoint and also nudge it forward during register accesses.

Internet says 30ms track settling time for 3.5, 40ms for 5.25.
