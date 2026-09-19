# Serial ports — implementation spec

Developer spec for GSSquared serial attachments and RS-232 handshake inputs. User guides: [Serial & Parallel Connections](SerialConnections.md), [Serial / Modem](Serial_Modem.md), [Super Serial Card](Cards_SuperSerial.md).

## Mental model

A GSSquared `SerialDevice` is the **external device plus the correct cabling for that device** (null-modem vs straight-through, which handshake pins are wired). The emulated 6551 or SCC only sees what that cable would present.

- **File / Clipboard / Echo** — a sink (or loopback) on a ready cable: CD, CTS, and DSR stay asserted so guest firmware that waits for handshake does not stall.
- **Modem** — a Hayes box on a modem cable: DSR up while the “modem” is powered (device attached), CD only while the TCP session is up. `+++` stays online (CD stays up); `ATH` or a dropped socket drops CD.
- **Host serial** — the USB-UART / dongle with its real pins. CD / CTS / DSR are sampled from the host OS.

## Goal

Pass incoming handshake levels into the guest UARTs so BBS software (e.g. Warp6 + WiModem232, issue #179) sees carrier loss, while capture attachments keep looking “ready.”

## Layering

```
guest 6551 / SCC
        │  register access + per-frame poll
        ▼
  SerialDevice.modem_inputs()     ← atomic levels (not queue messages)
        ▲
        │  set_modem_inputs()
  SerialPortDevice / ModemDevice / (File, Clipboard, Echo stay at default)
        ▲
        │  HostSerial::get_modem_inputs()
  TIOCMGET / GetCommModemStatus
```

Bytes and baud still travel on the SPSC queues (`MESSAGE_DATA`, `MESSAGE_LINE`). Handshake pins are **levels** on `std::atomic<uint8_t>`. The chip compares to the last sample and synthesizes **edge-triggered, then latched** IRQs.

Do not put `MESSAGE_CD_UP` / `MESSAGE_CD_DOWN` on the data queue: a dropped hang-up would leave carrier asserted, and `update_queues()` cannot peek past RX data.

## Polarity

Host-logical bits on `SerialDevice` (1 = line asserted):

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `MODEM_CD` | Carrier present |
| 1 | `MODEM_CTS` | Clear to send |
| 2 | `MODEM_DSR` | Data set ready |

Default is `MODEM_INPUTS_ASSERTED` (all three on).

Chip inversions:

| Surface | Asserted (ready / carrier) |
|---------|----------------------------|
| 6551 `ST_DCD` (status bit 5) | **0** (1 = no carrier) |
| 6551 `ST_DSR` (status bit 6) | **0** (1 = not ready) |
| SSC DIPSW2 bit 0 (CTS) | **0** (1 = not ready) |
| SCC RR0 `r0_dcd` / `r0_cts` | **1** |

SCC RR0 has no DSR bit. DSR is carried for the 6551 only; it is not mashed onto SCC CTS.

## Device policy

| Device | CD | CTS | DSR |
|--------|----|-----|-----|
| **SerialPortDevice** | live host CD | live CTS | live DSR |
| **ModemDevice** | 1 iff TCP socket exists | 1 | 1 |
| **File / Clipboard / Echo** | 1 (default; device does not write) | 1 | 1 |

SerialPortDevice starts and stays at `0` while the host port is detached (unplugged / open failed). First sample after attach is published immediately so a WiModem that boots with CD off is not stuck at the chip default.

Keep `CLOCAL` on POSIX host UARTs: loss of CD must not close the fd. Linux `POLLHUP` remains non-fatal.

## IRQs

Status bits follow the **current level**. IRQ fires only when a sampled bit **changes**, then latches until the guest clears it. Holding CD down does not keep re-raising IRQ.

- **6551:** CD or DSR edge, if command DTR is ready and DIP SW2-6 IRQ is enabled. Cleared by reading status.
- **SCC:** CD edge if WR1 Ext IE and WR15 DCD IE; CTS edge if WR15 CTS IE. Sets that channel’s Ext/Status IP (RR3). Cleared by WR0 Reset Ext/Status Int.

Chips sample on register access and once per video frame so interrupt-driven hang-up still works if the guest is not touching the UART.

## Out of scope

- Guest → host DTR / RTS (ATH-by-dropping-DTR on a real WiModem)
- IIgs firmware “DSR handshake” as a separate SCC pin (RR0 has none)

## Related

- [Serial & Parallel Connections](SerialConnections.md)
- [Serial / Modem](Serial_Modem.md)
- [Super Serial Card](Cards_SuperSerial.md)
- [SSC.md](SSC.md)
- [SCC8530_Serial.md](SCC8530_Serial.md)
