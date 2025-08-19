# Networking

Several forms of networking are contemplated:

* Uthernet II emulation
* Serial-to-telnet emulation
* FujiNet

## Uthernet II

There is a reference that this card has a chip with a built-in TCP stack, and the Uthernet manual discusses programming Socket Registers etc. However you can also do "mac raw sockets" so it's unclear if application software uses the hw TCP or its own.

Assuming applications use BOTH, then we have to emulate both a raw packet interface as well as TCP and UDP sockets. While there are a fair number of moving parts, the chip's interface seems rational and clean and it shouldn't be hard to deliver a correct solution.

## Serial-to-telnet

In some ways this is quite a lot simpler. What needs to be emulated here is two things:

1) a super serial card
2) a hayes-compatible modem that is a telnet client (&/or server)

This is a pretty simple one.


## FujiNet

ok there's a third one. FujiNet seems to be mostly organized around providing access to TNFS (trivial network file system) stores for disk images; 
There seems to be little need to do this, since the emulator is running on a machine that can access any network file system transparently.

There is also support for operating as a "character device", which can emulate a printer, or a modem. What Apple II software supports the SmartPort character interface?
Apparently it has a built-in terminal emulator program.
the emulated modem is hayes-compatible ATDT etc.

It may also offer a generic TCP/IP interface for certain apps. telnet and ssh. using program CATER. But other internet applications are supported.

There is a port of fujinet's firmware called fujinet-pc that works on mac/linux/pc: https://github.com/a8jan/fujinet-pc . This seems to run as a separate process on your local computer, then your emulator can connect to it.

"FujiNet utilizes a protocol called NetSIO to bridge between an emulator (like atari800) and the FujiNet device over a network connection. "

Now this sounds like you can use a real FujiNet device over ethernet using NetSIO. Or, alternatively, use FujiNet-PC in lieu of a physical device.

So in theory what could be done here is: implement SmartPort devices that talk the NetSIO protocol to a collocated instance of FujiNet-PC (or, to one you run separately on your computer). I like the collocated idea tho. This could be a lot of bang for the buck.

[ ] Control-Shift-2 and Control-Shift-6 don't seem to do the right stuff - should create an ASCII 0x00 and ??? needed for Lode Runner cheat.
