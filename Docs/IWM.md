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

