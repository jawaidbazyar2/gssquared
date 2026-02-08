# SCC8530 Serial Ports

On the IIgs the SCC85C30 chip provides two serial ports for the GS.

Typical applications are:

1. connect to a printer
1. connect to a modem
1. connect to an appletalk network

These functions are inherently I/O bound, and rather than try to implement them in the main emulator loop, we'll start a separate real OS thread, so that the "modem" thread for instance won't cause the OS to put the main loop process to sleep.

AppleTalk is a dead, legacy protocol. While someone might get a kick out of connecting a GS to an ancient Mac via AppleTalk, it would be a huge amount of work for very little benefit.

So we skip for now. However, the architecture described herein would be amenable to an AppleTalk option.

## Clocking

The master clock into the SCC is 3.6864MHz. We need to know this to determine how to interpret the time constants.

## Communication Modes

The SCC supports various synchronous modes, to be used for HDLC, SDLC, etc. These will never be engaged on an Apple IIgs, and we can disregard this functionality.

The SCC "contains facilities for modem controls" this will be things like hangup, DSR/DTR, etc.

* CTSA, CTSB
* DCDA, DCDB
* IEI
* IEO
* INT
* INTACK
* RxDA, RxDB
* TxDA, TxDB
* DTxCA, RTxCB
* RTSA, RTSB
* SYNCA, SYNCB
* TRxCA, TRxCB
* W/REQA, W/REQB

There is a 10 x 19 Status FIFO; a 3 byte Rec error FIFO, another 3 byte Rec error FIFO;

## Registers

There are 16 write registers, and 8 read registers.

WR4A - write register 4 for channel A
RR3 - read register 3 for either / both channels

### SCC Read Register Functions

For each channel:

| Read Reg | Description |
|-|-|
| RR0 | Transmit / Receive buffer status and external status |
| RR1 | Special Receive Condition status |
| RR2 | Modified interrupt vector (Channel B only); Unmodified interrupt vector (Channel A only) |
| RR3 | Interrupt Pending Bits (Channel A only) |
| RR8 | Receive buffer |
| RR10 | Miscellaneous Status |
| RR12 | Lower byte of baud rate generator time constant |
| RR13 | Upper byte of baud rate generator time constant |
| RR15 | External / Status interrupt Information |

### SCC Write Register Functions
| Write Reg | Description |
|-|-|
| WR0 | CRC Initialize; init commands for various modes, register pointers |
| WR1 | Transmit / receive interrupt and data transfer mode definition |
| WR2 | Interrupt vector (accessed through either channel) |
| WR3 | Receive Parameters and control |
| WR4 | Transmit / Receive misc parameters and modes |
| WR5 | Transmit parameters and controls |
| WR6 | Sync charatcers or SDLC address field |
| WR7 | Sync character or SDLC flag |
| WR7* | Extended feature and FIFI control (WR7 Prime) - 85C30 only |
| WR8 | Transmit buffer |
| WR9 | master interrupt control and reset (accessed through either channel) |
| WR10 | Misc transmitter / receiver control bits |
| WR11 | Clock mode control |
| WR12 | Lower byte of baud rate generator time constant |
| WR13 | Upper byte of baud rate generator time constant |
| WR14 | Misc control bits |
| WR15 | External Status / Interrupt Control |

### WR0

| bits | values | description |
|-|-|-|
| 7-6 | 1 1 | Reset Tx Underrun/EOM Latch |
| 7-6 | 1 0 | Reset Tx CRC Generator |
| 7-6 | 0 1 | Reset Rx CRC Checker |
| 7-6 | 0 0 | null code |
| 5-3 | 0 0 0 | null code |
| 5-3 | 0 0 1 | point high |
| 5-3 | 0 1 0 | reset ext/status interrupts |
| 5-3 | 0 1 1 | send abort (SDLC) |
| 5-3 | 1 0 0 | enable int on next rx character |
| 5-3 | 1 0 1 | reset tx int pending |
| 5-3 | 1 1 0 | error reset |
| 5-3 | 1 1 1 | reset highest IUS |
| 2-0 | x x x | Register number, plus combined with point high can get all 16 registers

### WR1

| bits | values | description |
|-|-|-|
| 7 | x | Wait/dma request |
| 6 | x | wait/dma request function |
| 5 | x | wait/dma request on receive/transmit |
| 4-3 | 0 0 | Rx Int Disable |
| 4-3 | 0 1 | Rx int on first character or Sp Cond |
| 4-3 | 1 0 | Int on all Rx characters or Sp Cond |
| 4-3 | 1 1 | Rx Int on Sp Cond Only |

### WR2

Interrupt vector bits 7-0 (V7 - v0).

### WR3

| bits | values | description |
|-|-|-|
| 7-6 | 0 0 | Rx 5 bits/char |
| 7-6 | 0 1 | Rx 6 bits/char |
| 7-6 | 1 0 | Rx 7 bits/char |
| 7-6 | 1 1 | Rx 8 bits/char |
| 5 | x | Auto Enables |
| 4 | x | Enter Hunt Mode |
| 3 | x | Rx CRC Enable
| 2 | x | Address search Mode (SDLC) |
| 1 | x | Sync Character Load Inhibit |
| 0 | x | Rx Enable |

### WR4

| bits | values | description |
|-|-|-|
| 7-6 | 0 0 | X1 Clock Mode |
| 7-6 | 0 1 | X16 Clock Mode |
| 7-6 | 1 0 | X32 Clock Mode |
| 7-6 | 1 1 | X64 Clock Mode |
| 5-4 | 0 0 | 8-bit Sync Char |
| 5-4 | 0 1 | 16-bit Sync Char |
| 5-4 | 1 0 | SDLC Mode |
| 5-4 | 1 1 | External Sync Mode |
| 3-2 | 0 0 | Sync Modes Enable |
| 3-2 | 0 1 | 1 Stop bit |
| 3-2 | 0 0 | 1.5 Stop Bit |
| 3-2 | 0 0 | 2 stop bit |
| 1 | x | Parity Even /ODD |
| 0 | x | Parity Enable |

### WR5

| bits | values | description |
|-|-|-|
| 7 | x | DTR |
| 6-5 | 0 0 | Tx 5 Bits |
| 6-5 | 0 1 | Tx 6 Bits |
| 6-5 | 1 0 | Tx 7 Bits |
| 6-5 | 1 1 | Tx 8 Bits |
| 4 | x | Send Break |
| 3 | x | Tx Enable |
| 2 | x | /SDLC CRC16 |
| 1 | x | RTS |
| 0 | x | Tx CRC Enable |

### WR6, WR7, WR7'

Used for Sync operation only

### WR9

| bits | values | description |
|-|-|-|
| 7-6 | 0 0 | No Reset |
| 7-6 | 0 1 | Channel Reset B |
| 7-6 | 1 0 | Channel Reset A |
| 7-6 | 1 1 | Force HW Reset |
| 5 | x | Software INTACK Enable (must use this) |
| 4 | x | Status High /LOW |
| 3 | x | MIE (master interrupt enable) |
| 2 | x | DLC |
| 1 | x | NV |
| 0 | x | VIS |

### WR10

| 1 | x | Loop Mode |

### WR11

7: RtxC Xtal /noxtal
6-5: receive clock setting
4-3: tx clock setting
2-0: trxc output selection

### WR12-13 

Time Constant Lo, Hi

### WR14

| bits | values | description |
|-|-|-|
| 7-5 | 0 0 0 | null command |
| 7-5 | 0 0 1 | enter search mode |
| 7-5 | 0 1 0 | reset missing clock |
| 7-5 | 0 1 1 | disable dPLL |
| 7-5 | 1 0 0 | set source = BR Generator |
| 7-5 | 1 0 1 | set source = RTxC |
| 7-5 | 1 1 0 | set FM mode |
| 7-5 | 1 1 1 | set NRZI mode |
| 4 | x | Local Loopback |
| 3 | x | Auto Echo |
| 2 | x | /DTR / Request function |
| 1 | x | BR Generator Source |
| 0 | x | BR Generator Enable |

### WR15

Interrupt enables

| bits | values | description |
|-|-|-|
| 7 | x | Break/Abort IE |
| 6 | x | Tx Underrun/EOM IE |
| 5 | x | CTS IE |
| 4 | x | Sync/hunt IE |
| 3 | x | DCD IE |
| 2 | x | SDLC FIFO Enable |
| 1 | x | Zero Count IE |
| 0 | 0 | 0 nothing |


### R0 - Status (potential interrupt status)

| bits | values | description |
|-|-|-|
| 7 | x | Break/Abort |
| 6 | x | Tx Underrun/EOM |
| 5 | x | CTS (also DSR?) |
| 4 | x | Sync/hunt |
| 3 | x | DCD |
| 2 | x | Tx Buffer Empty |
| 1 | x | Zero Count |
| 0 | x | Rx Char Available |

### R1 - Status

| bits | values | description |
|-|-|-|
| 7 | x | End of Frame (SDLC) |
| 6 | x | CRC/Frmaing Err |
| 5 | x | Rx Overrun Err |
| 4 | x | Parity Error |
| 3 | x | Residue Code 0 |
| 2 | x | Residue Code 1 |
| 1 | x | Residue Code 2 |
| 0 | x | All Sent |

### R2 - interrupt vector

RR2 contains either the unmodified interrupt vector (Channel A) or the vector
modified by status information (Channel B). 

### R3 - Interrupt Pending

Always 0's in B channel. (So, only read in A Channel)

| bits | values | description |
|-|-|-|
| 7 | 0 |  |
| 6 | 0 |  |
| 5 | x | Channel A Rx IP |
| 4 | x | Channel A Tx IP |
| 3 | x | Channel A Ext/Status IP |
| 2 | x | Channel B Rx IP |
| 1 | x | Channel B Tx IP |
| 0 | x | Channel B Ext/Status IP |

### R10

| 7 | x | one clocks missing |
| 6 | x | two clocks missing |
| 4 | x | Loop Sending |
| 1 | x | On Loop |

### R15

Interrupt enables

| bits | values | description |
|-|-|-|
| 7 | x | Break/Abort IE |
| 6 | x | Tx Underrun/EOM IE |
| 5 | x | CTS IE |
| 4 | x | Sync/hunt IE |
| 3 | x | DCD IE |
| 2 | 0 | 0 |
| 1 | x | Zero Count IE |
| 0 | 0 | 0 nothing |

That all is a whole lot of bits, most of which we don't need.


## Use Modes

The two use modes are polling (all interrupts off) or interrupts mode - vectored and nested interrupts.

Each of the 6 sources of interrupts in the SCC (Transmit, Receive, and External/Status interrupts in both channels) has 3 bits associated with the interrupt source:

(So, 18 bits).

Interrupt Pending; Interrupt Under Service; Interrupt Enable.
If IE is set, then that source can request interrupts. (except when MIE master interrupt enable is reset, then no interrupts).
The IE bits are write-only.

the other two bits are related to interrupt priority chain - this is a little fuzzy. need to see how this is implemented in the GS.

Channel A has higher interrupt priority than Channel B, and Receive - Transmit - External/Status - are prioritized in that order (highest to lowest).

Receive interrupt modes:
* interrupt on first receive character (or special receive condition)
* interrupt on all receive characters (or special receive conditions)
* interrupt on special receive conditions only.

## Async Modes

Send and receive are independent on each channel, with 5 to 8 bits per character, plus optional even or odd parity.
1, 1.5, 2 stop bits, and can provide a break output.
The receiver break detection can interrupt both at start , and at end, of a break.

Transmit and receive clocks don't have to be symmetric.

The Bitrate Generators start at the value, count down to 0, and this toggles a square wave from lo to hi or vice-versa.
the output of the BRG can be the transmit clock, the receive clock, or both.

Time Constant = (PCLK / (2 * Baud Rate * Clock Mode)) - 2

or: 

Time Constant + 2 = PCLK / (2 * Baud Rate * Clock Mode)
Baud Rate = PCLK / 2 * clock mode * (time constant + 2)

the clock mode is 1, 16, 32, or 64 programmed in a register, and Async modes use 16, 32, or 64.

So PCLK here is that 3.6864MHz.

## Data Encoding

NRZ, FM1, FM0, Manchester. I don't think we care about these.

## Auto Echo and Local Loopback

the SCC can automatically echo everything it receives. 

## Receive Status FIFO

Used primarily for SDLC. Hopefully we don't have to fuss with this.

# Register Selection, Reading/Writing

First step is to write WR0, which contains 3 bits that point to the selected register.
The second write is then the actual control word for the selected register.
(same with read, it's a Write to WR0 then a read).
The pointer bits are automaticalyl cleared after a read or write so that WR0/RR0 is addressed again.
ok that is pretty straightforward.

# IIgs Implementation

IEI, INTACK are tied high.
INT.L goes to the interrupt combiner doohickey.
SEL tied low.
D0-7 connect to data bus. Address decode logic feeds A/B, C/D.
PCLK is NOT the 3.68. It's tied to RTXCA. The other side tied to RTXCB and SYNCA.
I am unclear what PCKL (CREF) is. It's next to 14M and 7M so maybe it's the color burst signal at 3.579MHz.
interesting so that other thing is 3.68MHz and this is 3.579MHz.. hmm..
PCLK is for the chip, the other clock is for the communication lines.

# Development Plan

First, establish variables to store the registers, and be able to read/write them. This means we'll do the union / bitfield setup that is working well elsewhere.

Then do calculations to go from Multiplier / Time Constants, to a baud rate and a "14M's to next event".

Then, ought to be able to write a simple program to just stuff bytes out the port.
Then try something simple like PR#1, to a simulated printer.

Ah, of course, the ROM interrupt handler constantly pounds this thing to see if it's the source of an interrupt.

Slot 2 = Modem = B
Slot 1 = Printer = A


# External Devices

We need to model these various external devices. Each of these devices is a separate thread.

Aside from character queues, we need a way to communicate status, configuration changes, etc.

The main loop communicates with the thread through the use of thread-safe queueing.
A buffer of appropriate size is created, and head and tail pointers maintained. the main loop must never block on either queue - if a queue is exhausted, the data must be discarded.
The thread can, however, implement flow control and should typically do so by pushing the blocking decision towards the operating-system side. I.e., waiting on a TCP socket, opening/closing/writing disk files..

Create a queueing class to manage the in's and out's of communicating with these modules.

## File

The constructor is called with the file name. 
On first instance, just store the info.
On reset, or DSR/DTR (whatever the heck it is) close the file.

If the file is closed, and a character is written, open the file.

## Modem / Socket (clear)

A cleartext socket, e.g., telnet. Captain's Quarters is port 6800, and plain text telnet. 
Also- a simple Hayes command set modem emulator.
For simple testing, just try opening a socket to port 23 on 192.168.0.96 now.

## Printer

Somehow, print to a network printer. Somehow select the printer. Then it's really just telnet to the printer.

## Echo

This is for my initial testing. Once the interrupts etc are tested ok, move the echo functionality to a newly created SerialDevice class, which will run in a separate thread (or will it?)

## SSL Socket

An SSL encrypted socket - or, do we have to go full ssh?

## Serial Port

I suppose we could support serial ports. This would be similar to file, and on sensible operating systems we can just access the serial port using a file name! With some extra ioctl's for configuring the port.

## Messages

So we need to be able to send both control and data messages to these processes. we can make the messages a 16-bit word.
Since this is a serial emulator, I guess there would not need to be many. Basically the serial port signals.

SDL3 AsyncIOQueue construct may be exactly what we're looking for here. the thread side could sleep. the main side could poll.

### BREAK

to send a brk, set Reg5[4] = 1 and hold it for some time. Proterm apple-B does this.
So we should have BREAK:ON and BREAK:OFF messages.

### RATE

sets the effective data rate for the link. So all devices automatically follow the baud rate setting.

## Code Structure

base class: SerialDevice

the constructor will fire off 
we'll have basic routines:
  host_read, host_write, host_message
  device_read, device_write, device_message
These will use the various message queues.

these are for the host to read, write; and for the (threaded) device code to read.
destructor will:
  send shutdown message to the device
  wait for the device thread to end
  
Timing: timing can be handled by the base class, on the host side. That way:
1. devices can just run at full speed and be limited by queue backpressure
1. also keeps device code simpler
1. it's all managed by the host side
1. but the same code is used in one place, in the base class.
1. will be in main thread with cpu as timer callbacks will get called regularly.
1. however, if a device wants to and it makes sense (e.g., printers) it can disregard the base class event scheduling stuff and just cram data out as fast as possible.

baud rate / (data bits + start + stop + parity bits) = "word size" we use for timing.
e.g. 38400 baud / (8 + 1) = 4266 bytes per second, or, each 3356 14M's.
2400 baud / (8+1) = 266 bytes/sec = each 54000 14M's.

This is pretty interrupty. it could be a lot of context switches. We could back off on this somewhat and let data buffer in the queue. then we are not context switching all that often.

Can call less often, but process 2x chars at a time? Just thinking of ways to reduce "interrupt" overhead. 




## Feb 3, 2026

just doing a pr#1, we're hanging. It's reading C039 (sccareg), and checking bits. ok, reg 0 returning:
DCD=1, CTS=1, Tx Buffer Empty=1, and we're working! (with pr#1 and pr#2 from basic).

in GS/OS, we're in different code. FF/5100, FF/B84C

2400 baud is reg12=2E. 

At one point the firmware was setting baud rate etc but now it's not after I added some r/w for another register. In fact there's a lot of reading Reg 0 and then a fat lot of nothing. Let the fun begin!

Key functions involved: WRITESCC (X is register, A is value, Y is channel). Literally all that does is:
3C - channel A, 3B - channel B. Load from BFFD,y. Now why would they do that. OY. It's to avoid the PHANTOM LORD (air guitar solo), er, I mean, the phantom read.

## Feb 7, 2026

got a few programs doing basic send/receive now and not barfing. That's a good start.


# Compatibility / Testing

ProTerm wants interrupts, according to the Interwebs.

SCC: Register Select Ch: 0 -> New Reg 3
SCC: WRITE register: Ch 0 / Reg 0 = 03
SCC: READ register 3: Ch 0
SCC: READ register: Ch 0 / Reg 3 = 00
SCC: READ register 0: Ch 0
SCC: READ register: Ch 0 / Reg 0 = 2C
SCC: WRITE register 0: Ch 0 = 03

Teleworks also uses interrupts. Bit 3 of Reg9 is set. Well I Guess I can just enable interrupt support.

because interrupts can happen any time, we need to provide the class a callback, that will update the interrupt status. It's that or pass in the CPU which is icky icky poo poo.

Practically speaking, we should separate the IRQ management details from the cpu, so there will be an InterruptController. It's the only thing that will talk to the CPU (a single IRQ line to pull down). And all other devices that use IRQs go there. And it will live in computer, so devices can snatch it out of computer when they start.

This is also gonna need a 14M eventTimer; the existing CPU-based eventTimer is more or less useless on a GS, so other eventTimer users should be switched over to the 14M also. Mockingboard is the biggie; it would be interesting if it continued to clock at 1MHz (because that's PH0 on the bus) but timing by the 14M. Then Ultima music would operate at the right speed regardless of CPU speed. That's how it would work in a GS, I think.

pr#1 is hanging because when we write a char to it, it is never trying to read (why would it) and thus never clears the echo'd character. So be wary of that for now.
You should run GS2 with the mount disk options bro.

Ah ha!! once I fixed RR3 (only exists in Ch A) I can echo characters at my leisure in Teleworks.

oddly, ProTERM is still giving me back garbage characters. I can see that it's reading the correct characters back. Let's try another program.

ANSITerm Demo also works, tho there are two places I have to manually read the read Reg to get it to clear the interrupt and continue.

Another possibility is that the interrupt comes back -too fast- for the code to do anything. This may be helped by implementing correct send and receive timing. that will be easier with the separate device.

Telcom pretty much immediately blows up with a BRK. What the hell is that guy doing lolz. Does it require installing a custom serial driver?

FreeTerm also working. 
