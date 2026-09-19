# Serial / Modem

GSSquared includes a virtual Hayes-compatible modem you can attach to a serial port. From Apple II terminal software it behaves like dialing out over a phone line — except the “number” is a TCP host and port on the internet.

## What you need

1. A serial port: IIgs built-in SCC, or a [Super Serial Card](Cards_SuperSerial.md) in a slot.
2. That port’s attachment set to **Modem** — see [Serial & Parallel Connections](SerialConnections.md).

## How to dial

Use any normal serial program (ProTERM, Spectrum, TelCom, etc.). Example:

```
ATDTcqbbs.ddns.net:6800
```

That connects to host `cqbbs.ddns.net`, TCP port `6800`.

## How to hang up

Send the Hayes escape sequence, then hang up:

```
+++
ATH
```

(Wait about a second of silence before and after `+++`.)

`+++` returns to command mode but **keeps carrier detect asserted** (the TCP session is still up). `ATH`, a failed dial, or a dropped socket deasserts CD so guest software that watches 6551 `ST_DCD` or SCC RR0 DCD sees hang-up. DSR stays asserted whenever the virtual modem is attached (powered). See [SerialPortSpec.md](SerialPortSpec.md).

## File capture, clipboard, or a real serial port

Set the port attachment to **File** to save serial output to a host file (a toast shows the filename when the file closes). **Clipboard** copies the captured/printed text to the host clipboard on close. **Serial** attaches a real host serial port. Details: [Serial & Parallel Connections](SerialConnections.md).

## Related

- [Super Serial Card](Cards_SuperSerial.md)
- [Serial & Parallel Connections](SerialConnections.md)
- [Serial port spec](SerialPortSpec.md)
- [Uthernet II](Cards_UthernetII.md) — full TCP/IP without a serial modem
