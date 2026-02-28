# ProDOS Block Device

The purpose of this is to provide a simple block device interface that follows the ProDOS Block Device Interface standard.

## Registers

| Address | Name | Description |
|---------|------|-------------|
| $C0n0 | CMD_RESET | reset command block. |
| $C0n1 | CMD_PUT | write a byte of command block to the device. |
| $C0n2 | CMD_EXECUTE | execute the command block. |
| $C0n3 | ERROR_GET | get error status from last command. |

The goal of this design is to provide a simple interface for device drivers, and, to provide some operational security against accidentally writing command block data and causing data corruption on the device.


## Command Blocks

A command block is a series of bytes written 

* Version 1 Command Block

| Byte | Description |
|------|-------------|
| 0x00 | Command Version |
| 0x01 | Command |
| 0x02 | Device |
| 0x03 | Address (low) |
| 0x04 | Address (high) |
| 0x05 | Block (low) |
| 0x06 | Block (high) |
| 0x07 | Checksum |

Checksum is the xor of all the bytes in the command block, from 0x00 through the byte preceding the checksum. For Version 1, it's the xor of bytes 0x00 through 0x06.


Values 0x01 through 0x06 mirror the values at $0042 to $0047.

```
#define PD_CMD        0x42
#define PD_DEV        0x43
#define PD_ADDR_LO    0x44
#define PD_ADDR_HI    0x45
#define PD_BLOCK_LO   0x46
#define PD_BLOCK_HI   0x47
```

## Command Versions

| Version | Description |
|---------|-------------|
| 0x01 | Version 1 |


## Error Status

| Value | Description |
|------|-------------|
| 0x00 | No error |
| 0xFC | Invalid Command block |

## Other errors:

https://prodos8.com/docs/technote/21/

| Code | Error |
|--|--|
| $27 |  I/O error |
| $28 |  No Device Connected |
| $2B |  Write Protected |
| $2F |  Device off-line |
| $45 |  Volume directory not found |
| $52 |  Not a ProDOS disk |
| $55 |  Volume Control Block full |
| $56 |  Bad buffer address |
| $57 |  Duplicate volume on-line |

$2F is what we should return if a drive is present, but there is no media in it.
There is also the potential for error $2B (write protected).

# Adding SmartPort

ok we need to add SmartPort support to this device. Initially, just two drives, Ultimately, to allow loading up multiple (many) partitions at once like for a hard disk.

The current firmware area ($CSxx) has only 70 bytes left. Not sure I can get it done in that. So will likely have to add a C800 ROM to it. map it like so?

$0000 => $CS00
$0100 => $C900 ProDOS Block Driver
$0200 => $CA00 SmartPort standard
$0300 => $CB00 SmartPort extended

etc as needed.

Other items to address:

JMP $E000 => instead of dumping to AppleSoft, we should daisy-chain to next slot down. There's a ROM address you can jump to instead for that I heard.
How does caller know whether extended calls are supported? Ah. CnFB[7] = 1 if you support extended. That's easy-peasy. So Apple IIe stuff just won't look for that.

## Debugging Airheart

So the existing ProDOS Block interface appears to be working just dandy, and most stuff is just using that up until airheart loads its screen, then does what now?

cmdnum: 1
pcount: $9E
unit: $5
addr_lo: 0x60

addr_hi: 0x03
block: 0x450001

say what now?
ah I'm copying from the wrong place into this thing.
cmd 1 is indeed read.
but that's then address 059E. I am missing an indirection.

AirHeart and a few other titles were assuming a ProDOS block device was a SmartPort device (didn't check the 4th ID byte). So they were calling a non-existent handler!

AirHeart now working.
