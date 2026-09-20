# Serial / Modem

GSSquared includes a virtual Hayes-compatible modem you can attach to a serial port. From Apple II terminal software it behaves like a phone-line modem — except outbound “numbers” are TCP hosts, and inbound telnet on port **6502** rings the guest.

## What you need

1. A serial port: IIgs built-in SCC, or a [Super Serial Card](Cards_SuperSerial.md) in a slot.
2. That port’s attachment set to **Modem** — see [Serial & Parallel Connections](SerialConnections.md).

## How to dial

Use any normal serial program (ProTERM, Spectrum, TelCom, etc.). Example:

```
ATDTcqbbs.ddns.net:6800
```

That connects to host `cqbbs.ddns.net`, TCP port `6800`.

Result codes default to words (`OK`, `CONNECT 9600`, `RING`, `NO CARRIER`, `ERROR`). `ATV0` switches to Hayes numeric codes; `ATV1` restores words. `AT&F` also restores `V1`.

Numeric (`V0`): `0` OK, `1` CONNECT (300 / unknown), `2` RING, `3` NO CARRIER, `4` ERROR, `5`/`10`/`11`/`12`/`14`/`28` CONNECT at 1200 / 2400 / 4800 / 9600 / 19200 / 38400 (Hayes Smartmodem + GBBS).

## How to answer

With **Modem** attached, GSSquared listens on TCP port **6502** (all host interfaces). When a telnet client connects (for example `telnet <your-host> 6502`), the guest sees Hayes **`RING`** — immediately, then about every 3 seconds — and carrier stays down.

Answer from the terminal program:

```
ATA
```

(`ATA0` is the same.) The modem raises CD, sends `CONNECT <baud>`, and the telnet session is fully connected. If nothing is ringing, `ATA` returns `NO CARRIER`.

`ATH` (or `ATZ` / `AT&F`) while ringing drops the inbound caller and returns `OK`. If the caller hangs up before you answer, `RING` stops and there is no `NO CARRIER` (the modem never went off-hook).

`ATS0=n` auto-answers after **n** `RING` results (`ATS0=1` on the first ring). `ATS0=0` (default, also after `AT&F`) turns auto-answer off so the guest must send `ATA`. `ATS0?` reports the current value.

Only one ModemDevice can bind 6502. A second serial port also set to **Modem** stays outbound-only (the console logs the listen failure). Extra inbound connections while already ringing or online are refused.

## How to hang up

Send the Hayes escape sequence, then hang up:

```
+++
ATH
```

(Wait about a second of silence before and after `+++`.)

`+++` returns to command mode but **keeps carrier detect asserted** (the TCP session is still up). `ATO` (or `ATO0`) goes back to data mode and sends `CONNECT` again. `ATH`, a failed dial, or a dropped socket deasserts CD so guest software that watches 6551 `ST_DCD` or SCC RR0 DCD sees hang-up. DSR stays asserted whenever the virtual modem is attached (powered). See [SerialPortSpec.md](SerialPortSpec.md).

## File capture, clipboard, or a real serial port

Set the port attachment to **File** to save serial output to a host file (a toast shows the filename when the file closes). **Clipboard** copies the captured/printed text to the host clipboard on close. **Serial** attaches a real host serial port. Details: [Serial & Parallel Connections](SerialConnections.md).

## Related

- [Super Serial Card](Cards_SuperSerial.md)
- [Serial & Parallel Connections](SerialConnections.md)
- [Serial port spec](SerialPortSpec.md)
- [Uthernet II](Cards_UthernetII.md) — full TCP/IP without a serial modem
