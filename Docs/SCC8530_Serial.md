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

## Action Items / Roadmap

[ ] simulate the various serial control signals with messages to the serial device, such as CTS, RTS, DSR/DTR, etc. In theory you can hangup a modem that way.  
[ ] clear out the various queues on a reset?  

[ ] Implement "file" serial device  
[ ] implement ImageWriter II emulation    

[ ] Refactor "parallel" card to use threads and be connectable to file or imagewriter.  


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

The constructor is called with the file name. Append a unique ID (time stamp?) to the end of filename.
On first instance, just store the info.
On reset, or DSR/DTR (whatever the heck it is) close the file.
If the file is closed, and a character is written, open the file.


## Modem / Socket (clear) (done)

A cleartext socket, e.g., telnet. Captain's Quarters is port 6800, and plain text telnet. 
Also- a simple Hayes command set modem emulator.
For simple testing, just try opening a socket to port 23 on 192.168.0.96 now.

uses telnet protocol. Is that the right choice?

## Printer

Somehow, print to a network printer. Somehow select the printer. Then it's really just telnet to the printer. There is no easy cross-platform stuff for printing.

Practically speaking, I think the thing to do is:
1) emulate an ImageWriter II (and/or some other printer)
2) generate a unique file name
3) generate a PDF file based on that.
4) call OS to open the PDF file with syd::system(command)

print job completion detection: wait for a significant pause in the data output to signal end of the job. (5 seconds? 10 seconds?) Have to see how that works with various programs.

To generate postscript output, have to use this modified version with DC Printer control panel.
http://www.apple2works.com/directconnectpostscriptdriver/

## Echo (done)

This is for my initial testing. Once the interrupts etc are tested ok, move the echo functionality to a newly created SerialDevice class, which will run in a separate thread (or will it?)

## SSL Socket

An SSL encrypted socket (just TLS) - or, do we have to go full ssh?

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

Telcom pretty much immediately blows up with a BRK. What the hell is that guy doing lolz. Does it require installing a custom serial driver? it's got the tools right in it, I think I load them manually..

FreeTerm also working. 


## Cross-platform snippet to open PDF file in viewer

```
#include <iostream>
#include <string>
#include <cstdlib> // Required for std::system

void open_pdf_file(const std::string& filename) {
#ifdef _WIN32
    // Windows: Use ShellExecute or the 'start' command
    std::string command = "start \"\" \"" + filename + "\"";
    std::system(command.c_str());
#elif __APPLE__
    // macOS (OSX): Use the 'open' command
    std::string command = "open \"" + filename + "\"";
    std::system(command.c_str());
#elif __linux__
    // Linux: Use xdg-open (common on many distributions)
    std::string command = "xdg-open \"" + filename + "\"";
    std::system(command.c_str());
#else
    std::cerr << "Unsupported operating system for automatic PDF opening." << std::endl;
#endif
}

int main() {
    std::string pdf_file_path = "path/to/your/local_document.pdf"; // Replace with your file path
    open_pdf_file(pdf_file_path);
    return 0;
}
```

## Going through Self Test 06

first thing is SCCRegRW test, which calls ResetSCC. This does:
```
80C: initial data
80D: Register number
80E: Serial Mask (mask out non r/w bits)
80F: Mask Data (data masked with mask)

C039:09    // register 9
C039:C0    // write C0

then we do RwChA with 80C:00 02 FF, mask is ff but data 0 so we write a 0.
save the masked value to 80F.
C039:00   // reg 2 Ch a -> 0 , So this is setting the interrupt vector to 0.
then we read reg 2, and compare the masked value to 80F.
That's 0 in both cases. So this particular test passes.
then we DEC 80C, and we're going to loop..
They are testing that we have a clean 8 bits we can read and write to channel A. 

0294 6934 AD 0D 08     REGRW    LDA   RegNo                    ;Actual R/W test
0295 6937 99 FD BF              STA   SCCCmd,Y                 ;Point to the Wr reg
0296 693A AD 0C 08              LDA   DATA                     ;Initially = 0
0297 693D 2D 0E 08              AND   SerMask                  ;Mask out non R/W bits
0298 6940 8D 0F 08              STA   MaskData                 ;Store for later comparison
0299 6943 99 FD BF              STA   SCCCmd,Y                 ;Write the register
0300 6946 AD 0D 08              LDA   RegNo
0301 6949 99 FD BF              STA   SCCCmd,Y
0302 694C B9 FD BF              LDA   SCCCmd,Y                 ;Read the register
0303 694F CD 0F 08              CMP   MaskData                 ;Compare
0304 6952 D0 07                 BNE   BadData
0305 6954 CE 0C 08              DEC   DATA
0306 6957 D0 DB                 BNE   RegRW                    ;Try $0-FF
0307 6959 F0 D6                 BEQ   ZRTS
0308 695B A9 80        BADDATA  LDA   #$80
0309 695D 60                    RTS   
```

We are immediately failing when it tries to read back FF from ChA[02]. 

ok got past that - now need to test channel B, register C (12, baud rate) the same way?
then channel A, register 12.
then register 13 (0d) on both channels..
then register 15 (F) with a mask of 0xFA.

We're clearing the register r/w test now, and likely failing on the Internal Loop test.
looks like it's going to do it on both channels.. smert.

So to do that, it sets up the registers like so:

```
0421 6A1C              TBL2     EQU   *
0422 6A1C 09 00                 DC B:9,$00                     ;Disable interupts
0423 6A1E 04 4C                 DC B:4,$4C                     ;X16 clk 2 Stp bits
0424 6A20 0B D0                 DC B:11,$D0                    ;Xtal RTxC
0425 6A22 0C 5E                 DC B:12,$5E                    ;Low Byte Time const
0426 6A24 0D 00                 DC B:13,$00                    ;Hi Byte Time const
0427 6A26 0E 13                 DC B:14,$13                    ;Loopback BR enable
0428 6A28 03 C1                 DC B:3,$C1                     ;Rx 8bits
0429 6A2A 05 6A                 DC B:5,$6A                     ;Tx 8bits Tx enable RTS
```
we're failing test 6, "all sent". 

We now pass the self-test. However, see above for remaining action items.

## more bugs

"Each of the six sources of interrupts in the SCC (Transmit, Receive, and External/Status
interrupts in both channels) has three bits associated with the interrupt source.
Interrupt Pending (IP), Interrupt Under Service (IUS), and Interrupt Enable (IE)."

"In the SCC, if the IE bit is not set by enabling interrupts, then the IP for that source is
never set."

"Also if the MIE Enable bit in WR9 is reset, no interrupts can be requested."

OHHHH. My 8530 manual is way more complete than the one I've been using. For instance, page 3-9 shows the exact bits on reset of each channel's registers. Well duhhhh. I'm going to have to over the registers again in detail using the book.

some notes:

forgot to implement MIE (master interrupt control)
tx interrupt is not merely the same s tx buf empty. It's the -transition- from tx buf non empty, to empty. So some of these are edge sensitive, not level sensitive. i.e.
if !tx_buf_empty then tx_buf_empt=true; tx_interrupt=true
need to implement the vector register
apparently it will only flag interrupts according to a priority hierarchy:
the register map is more complex than I implemented perhaps..

let's start with the reset. The chip is reset by pulling r and w low at same time and holding a bit. there's logic on the mobo for this, mixing reset and the normal r/w signal.
So ctrl-reset definitely resets. ok, I think we're good. Key elements:
hw reset a little different than soft reset. Soft reset 3 is same as hw reset. Reminder than WR9 is the same register, accessed through either channel.

next: verify the register map.
ah, it may be ok - the table 3-3 lists A/B as one of the bits, which is just channel select.
Let's modify the routines so it's a little clearer what happens with the shared registers (i.e., don't have ch as arg on those).
ok, WR2 and WR9 are shared and we will always store these values in channel A's array.
ok, these look right. For the read registers that are shadowed:
11 (shadow) = 15 (real)
14 (shadow0) = 10 (real)
9 (shadow) = 13 (real)

Interrupts

transmit: 
polling. disable transmit interrupts and poll transmit buffer empty bit in RR0. (So that bit is always active).
Another way of polling is to enable transmit interrupt, then reset MIE bit. then cpu may poll the IP bits in RR3A to determine when transmit buffer is empty.
the Tx Int Req has only one source. It can only be set when the transmit buffer goes from full to empty. This means the transmit interrupt will not be set until after the first character is written to the SCC.

reset:
    tx_irq_condition = false

in transmit character:
    do the transmit
    if (tx_buf_empty) tx_irq_condition = true // this covers case if buffer is non-empty and they overwrite it anyway
    update_interrupts()

update:
    if tx_irq_condition and tx_ie then tx_ip = true

    if MIE and any IPs then assert interrupt to cpu.
    tx_irq_condition = false

the Rx Interrupt request caused by a receive char available; or a special condition.
The Rx Char Avail interrupt is generated when a character is loaded into the FIFO and is ready to be read. (there's a 3-byte FIFO).
special conditions are: receive FIFO overrun; CRC/framing error; end of frame; parity.
Parity condition may be included or not based on WR1[2].

The external/status interrupts have several sources which are individually enabled in WR15. They are:
zero count; DCD; Sync/hunt; CTS; transmitter underrun/eom; break/abort.

Each source of interrupt in SCC has 3 control status bits: IE (interrupt enable), IP (interrupt pending), IUS (interrupt under service).
If the IE bit is set, that source may cause an interrupt request.
if the IE bit is reset, no interrupt request will be generated by that source.
the IP bit for a source may be set by the presence of an interrupt condition in the SCC and is reset directly by the processor,
or indirectly by some action the processor may take.
If the corresponding IE bit is not set, the IP for that source will never be set.

Interrupt status flowchart

The flowchart can be simplified somewhat, because the GS has /INTACK pulled high. But the first box, "interrupt condition exists".
I think IUS bits are primarily used with this /INTACK logic.
We may need some flags that are separate from the IP bits to indicate "interrupt condition exists". because setting IP may be masked by IE not being set.
seems to imply if an interrupt condition exists and we then enable an IE, we'll get an IP.

So far, the only one of these may be the Tx buffer empty since it's edge sensitive.

ok, I have moved the logic for updating the IP bits to update_interrupts (and a couple helper routines). 


Interrupt Source Priority (highest to lowest)

| source | channel | Vector Value |
|-|-|
| Rx | A | 
| Tx | A | 
| Ext/Status | A | 
| Rx | B | 
| Tx | B | 
| Ext/Status | B | 

the internal daisy chain links the six sources of interrupt in a fixed order. chaining the ius bits for each source.
while an ius bit is set, all lower-priority interrupt requests are masked off. during an intack cycle (not relevant).
when MIE is set, has same effect as pulling IEI pin low, disabling all IRQ. But the above says you can disable MIE and poll IP bits. OK, so the IP
bits still follow interrupt status.

one thing that is still unclear:
are the IP bits masked based on priority? I.e. if there is a Ch A IP set, are Ch B IPs suppressed?
(or is that only when using the INTACK scheme?)

vectors

| bit 1 | bit 2 | bit 3 | description |
|-|-|-|-|
| V3 | V2 | V1 | Status High/Status Low = 0 |
| V4 | V5 | V6 | Status High/Status Low = 1 |
| 0 | 0 | 0 | Ch B Transmit Buffer Empty |
| 0 | 0 | 1 | Ch B Ext/Status Change |
| 0 | 1 | 0 | Ch B Receive Char Avail |
| 0 | 1 | 1 | Ch B Special Receive Condition |
| 1 | 0 | 0 | Ch A Transmit Buffer Empty |
| 1 | 0 | 1 | Ch A Ext/Status Change |
| 1 | 1 | 0 | Ch A Receive Char Avail |
| 1 | 1 | 1 | Ch A Special Receive Condition |

Q: why is this a write register?
ah, because the CPU can tell the chip which vector it is. i.e., assume it's using this intack procedure. Whichever device is asserting, that vector number is put on the data bus.
So say you have 5 chips, you give each a distinct vector number.

Note: 321 is reverse bit order from 456!
WR9[4] is status high /status low.
WR9[0] is VIS bit - if set, vector returned from reading WR2 is modified depending on highest priority IP. This bit is ignored if the NV (WR9[1]) bit is set.
NV primarily tri-states the bus, but also affects vector read.

I think I am probably failing to deal properly with some SCC interrupts, confusing the firmware into not clearing VBL interrupts? Is that possible?

Some updates and improvements..

ah ok, there was a mistake, I had a bool for the tx_ip pending flag inside a union instead of separate! I saw that error several times and ignored it assuming it was the audio thing, turns out to have been a real problem.

I had claude integrate the eventtimer stuff. Seems to be actually working.. 

But print jobs are still hanging.

After all these changes to the SCC code, some things are improved, however, I am failing self test 06070000. This is "rx char available". 
