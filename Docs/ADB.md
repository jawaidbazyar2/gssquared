# ADB - Apple Desktop Bus

The ADB-related registers are:

• Keyboard Data register ($C000)
• Keyboard Strobe register ($C010)

• Modifier Key register ($C025)

• Mouse Data register ($C024)

• ADB Command/Data register ($C026)
• ADB Status register ($C027)

C000/C010 are legacy registers to emulate Apple //e. The others are new and ADB-specific.

C000/C010 must be implemented using the underlying ADB logic.

## ADB Command / Data Register

On a write:

The command (as defined below) is written in these 8 bits.

On a read:

| Bit | Name | Description |
|-|-|-|
| 7 | Response/status | When this bit is 1, the ADB microcontroller has received a response from an ADB device previously addressed. 0: No response.|
| 6 | Abort/CTRLSTB flush | When this bit is 1, and only this bit in the register is 1, the ADB microcontroller has encountered an error and has reset itself. When this bit is 1 and bit 4 is also 1, the ADB microcontroller should clear the key strobe (bit 7 in the Keyboard Data register at $C000). |
| 5 | Reset key sequence | When this bit is 1, the Control, Command, and Reset keys have been pressed simultaneously. This condition is usually used to initiate a cold start up |
| 4 | Buffer flush key sequence | When this bit is 1, the Control, Command, and Delete keys have been pressed simultaneously. This condition wil result in the ADB microcontroller's flushing al internally buffered commands. |
| 3 | Service request valid | When this bit is 1, a valid service request is pending. The ADB microcontroller will then poll the ADB devices and determine which has initiated the request |
| 2 - 0 | Number of data bytes returned | The number of data bytes to be returned from the device is listed here. |

## Keyboard Modifier Register - $C025

| Bit | Modifier Key Pressed |
|-|-|
| 7 | Command |
| 6 | Option |
| 5 | Modifier Latch updated, but no key pressed |
| 4 | Numeric keypad key |
| 3 | non-modifier key is being held down |
| 2 | Caps Lock |
| 1 | Control |
| 0 | Shift |

Bit 5=1 means these are "live" values.
I think this means if 5=0, then these values are latched and associated with the most recent key press? That is unclear.

These are pretty straightforward.

## Mouse Data register - $C024

| Bit | Description |
|-|-|
| 7 | 1=Mouse button is up, 0=Mouse button is down |
| 6 | Delta sign bit: =1, delta is negative; =0, delta is positive |
| 5-0 | Delta X/Y (with sign bit, up to +/- 31) |

Read register twice in succession; first to get X, second to get Y. If you get out of sync, you will be reading X for Y and vice-versa. Is there a reset for this?

## ADB Status Register - $C027

| Bit | Name | Modifier Key Pressed |
|-|-|-|
| 7 | Mouse Data Reg Full | =1, Mouse Data register at $C024 is full |
| 6 | Mouse int ena/dis | = 1, mouse interrupt is enabled, int generated when mouse register contains valid data |
| 5 | Data register full | =1, command/data register contains valid data |
| 4 | Data interrupt ena/dis | =1, generate interrupt when command/data reg contains valid data |
| 3 | Keybd data register is full | =1, kbd data ($C000) has valid data. =0, kbd register empty. is cleared when kbd data reg is read, or adb status is read |
| 2 | Keybd data register int ena/dis | =1, interrupt thrown when kbd register contains valid data |
| 1 | Mouse X Available | =1, a X-coord data is available; if =0, a Y-coord data is available |
| 0 | Command register full | =1 when command/data register is written to; =0 cmd register empty, cleared when cmd/data register is read |

## Commands / Signals

Command: message sent to a specific device
Signal: broadcast to instruct all devices to perform a function.

Commands:
* Listen (write to device)
* Talk (read from device)
* Device Reset 
* Flush

Signals:
* Attention
* Sync
* Global Reset
* Service Request

## Transaction

Attention / Sync
Address: 4 bits
Cmd: 2 bits: LISTEN
Register: 2 bits
Then, 2 to 8 bytes of data

| Address | Command Code | Register |   |
|-|-|-|-|
| 7 6 5 4 | 3 2 | 1 0 | **Command** |
| x x x x | 0 0 | 0 0 | Send Reset |
| A3 A2 A1 A0 | 0 0 | 0 1 | Flush |
| x x x x | 0 0 | 1 0 | Reserved |
| x x x x | 0 0 | 1 1 | Reserved |
| x x x x | 0 1 | 1 0 | Reserved |
| A3 A2 A1 A0 | 1 0 | r1 r0 | Listen |
| A3 A2 A1 A0 | 1 1 | r1 r0 | Talk |

### Listen (Addressed)

Requests device to store data being transmitted in specified internal register (0-3).
When the host addresses a device to listen, the device receives the next data packet from the host and places it in the appropriate register. (So, 'registers' can be multiple bytes).
If the addressed device detects another command on the bus before it receives any data, the original transaction is immediately considered complete. (This may never happen in practice in an emulator).

### Send Reset (Broadcast)

When a device receives this command, it clears all pending operations and data and inits to power-on state. This will reset ALL devices.

### Flush (Addressed)

Clear all pending commands from the device.

### Attention/Sync

start of every command. Does not need to be emulated.

### Global Reset

If bus held low for 2.8ms, all devices reset. Only initiated by host.

### Service Request

Sent by device, to inform host that a device requires service, for example, when there are data to send to the host.
Following any command packet, a requesting device can signal an SR by holding the bus low. The devices hold this until they have been served.
At that point, the Host polls all devices by sending a Talk Register 0 command beginning with last active device.
Only the device with data to send will respond to this Talk command.
The Mouse does NOT ever use this SR mechanism.

### Device Registers

0-2 are device specific.
Register 3 is Talk: status, and device address handler; Listen: status.

* Keyboard Register 0
  * Bit 15: Key Released
  * Bit 14-8: Keycode 2
  * Bit 7: Key Released
  * Bit 6-0: Keycode 1

* Mouse Register 0
  * Bit 15: Button Pressed
  * Bit 14: Moved Up
  * Bit 13-8: Y move value
  * Bit 7: always 1
  * Bit 6: Moved right
  * Bit 5-0: X move value

* Registers 1 and 2
  * basically unused by keyboard/mouse

* Register 3
  * Bit 15: reserved, must be 0.
  * Bit 14: exceptional event.
  * Bit 13: SR enable
  * Bit 12: Reserved, must be 0.
  * Bit 11-8: Device address.
  * Bit 7-0: Device handler.

* Device Handlers. As Listen Register 3 data,
  * $FF - initiate self-test.
  * $FE - change address field to new address sent in this message.
  * $FD - change address to new address "if activator is pressed"
  * $00 - change address and enable fields to new values sent by host

* Device addresses
  * $02 - Encoded Devices - Keyboard
  * $03 - Relative Devices - Mouse

The keycodes are scancodes, not ASCII:
https://github.com/szymonlopaciuk/stm32-adb2usb/blob/main/src/keymap.h


https://github.com/gblargg/adb-usb/blob/master/keymap.h



So, we need to implement just like the ADB microcontroller. 
1. Receive (and send) messages on an ADB Bus. Have a data structure for a queue of packets.
1. as packets come in, we present them to the ADB Command/Data registers.
1. But, we also process them to some degree, e.g. updating values for $C024, $C025, $C000, etc.

