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