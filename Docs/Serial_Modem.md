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

## File capture instead of a modem

Set the port attachment to **File** to save serial output to a host file. When the file closes, GSSquared shows a short toast with the filename. Details: [Serial & Parallel Connections](SerialConnections.md).

## Related

- [Super Serial Card](Cards_SuperSerial.md)
- [Serial & Parallel Connections](SerialConnections.md)
- [Uthernet II](Cards_UthernetII.md) — full TCP/IP without a serial modem
