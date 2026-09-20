# Serial ports — implementation spec

Developer spec for GSSquared serial attachments and RS-232 handshake inputs. User guides: [Serial & Parallel Connections](SerialConnections.md), [Serial / Modem](Serial_Modem.md), [Super Serial Card](Cards_SuperSerial.md).

## Mental model

A GSSquared `SerialDevice` is the **external device plus the correct cabling for that device** (null-modem vs straight-through, which handshake pins are wired). The emulated 6551 or SCC only sees what that cable would present.

- **File / Clipboard / Echo** — a sink (or loopback) on a ready cable: CD, CTS, and DSR stay asserted so guest firmware that waits for handshake does not stall.
- **Modem** — a Hayes box on a modem cable: DSR up while the “modem” is powered (device attached), CD only while an **answered or dialed** TCP session is up (`socket`, not a pending inbound). Inbound telnet on port 6502 sends `RING` with CD down until `ATA`. `+++` stays online (CD stays up); `ATO` returns to data; `ATH` or a dropped socket drops CD.
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
| **ModemDevice** | 1 iff answered/dialed `socket` (not pending RING) | 1 | 1 |
| **File / Clipboard / Echo** | 1 (default; device does not write) | 1 | 1 |

SerialPortDevice starts and stays at `0` while the host port is detached (unplugged / open failed). First sample after attach is published immediately so a WiModem that boots with CD off is not stuck at the chip default.

### Telnet role

`ModemDevice` plays a different telnet role per direction, tracked by `server_role_`:

| | Outbound `ATD` (client) | Inbound :6502 (server) |
|---|---|---|
| Offers on connect | `WILL`/`DO BINARY` | `WILL`/`DO BINARY`, `WILL ECHO`, `WILL SGA`, `DO SGA` |
| Peer `DO ECHO` | `WONT` (a client must never echo the host) | `WILL` (the BBS echoes) |
| Peer `WILL ECHO` | `DO` | `DONT` |
| `DO`/`WILL BINARY`, `SGA` | accept | accept |
| Guest `CR` → peer | verbatim | verbatim if `binary_tx_`, else `CR LF` |
| Peer `CR LF` / `CR NUL` → guest | verbatim | verbatim if `binary_rx_`, else single `CR` |

Without the server-side `WILL ECHO` + `WILL SGA`, a BSD telnet client stays in line mode with local echo: the BBS gets nothing until Return, single-keystroke menus never fire, and `mode character` at the `telnet>` prompt cannot rescue it because the refusal comes from us.

### Binary mode and file transfers

`binary_tx_` / `binary_rx_` track the agreed BINARY state per direction (`DO`/`DONT BINARY` for what we send, `WILL`/`WONT BINARY` for what we receive). NVT line-end rewriting is **off** on a direction that negotiated binary, so ZMODEM, XMODEM, and other 8-bit payloads pass through untouched; a client that refuses binary still gets the readable `CR LF` courtesy. `IAC` (`0xFF`) is always escaped as `IAC IAC`, which telnet requires even in binary mode.

The rewriting is also confined to the server role, so the tested outbound ProTERM path (M2 / M4 / M5) stays byte-transparent regardless of negotiation.

GBBS Pro emits its own `CR LF` pairs, so the translation is a no-op for it; the only bare `CR` it produces is the terminating `CR` of Hayes commands like `ATA`.

One authentic hazard survives: `+++` in a guest upload can still trip escape detection if it follows a 1-second gap, exactly as on real hardware.

Keep `CLOCAL` on POSIX host UARTs: loss of CD must not close the fd. Linux `POLLHUP` remains non-fatal.

## IRQs

Status bits follow the **current level**. IRQ fires only when a sampled bit **changes**, then latches until the guest clears it. Holding CD down does not keep re-raising IRQ.

- **6551:** CD or DSR edge, if command DTR is ready and DIP SW2-6 IRQ is enabled. Cleared by reading status.
- **SCC:** CD edge if WR1 Ext IE and WR15 DCD IE; CTS edge if WR15 CTS IE. Sets that channel’s Ext/Status IP (RR3). Cleared by WR0 Reset Ext/Status Int.

Chips sample on register access and once per video frame so interrupt-driven hang-up still works if the guest is not touching the UART.

## Out of scope

- Guest → host DTR / RTS (ATH-by-dropping-DTR on a real WiModem)
- IIgs firmware “DSR handshake” as a separate SCC pin (RR0 has none)

## Test matrix

Manual. **Pass** means the guest and the HUD agree; do not treat HUD-only CD as a connect. Fill **Result** with `pass` / `fail` / `partial` / `untested` / `n/a`. Date is `YYYY-MM-DD`.

**Oracle:** HUD Stats (lower right) shows `CMD` / `RNG` / `ONL` / `ESC`, `CD± CTS± DSR±`, and the TCP peer. Guest status is whatever the terminal or BBS actually displays. Console `ModemDevice:` lines are supporting evidence only.

**Default guest** for modem rows is ProTERM 3.1 unless noted. IIe uses SSC (6551). IIgs uses built-in SCC (modem port unless noted).

### ModemDevice + ProTERM

| # | Platform | Port | Scenario | Result | Date | Notes |
|---|----------|------|----------|--------|------|-------|
| M1 | IIgs | SCC modem | Init `AT&FE0S7=99` → `OK` | pass | 2026-09-19 | Chained `&F` / `E` / `S7=` |
| M2 | IIgs | SCC modem | Dial `AT&Q5N1DT<host>` → TCP up | pass | 2026-09-19 | HUD `ONL CD+` + peer |
| M3 | IIgs | SCC modem | Guest leaves “waiting for connect” | pass | 2026-09-19 | Needs `CONNECT` text, not only DCD |
| M4 | IIgs | SCC modem | Online: type / remote echo | pass | 2026-09-19 | “ProTERM + ModemDevice seems ok” |
| M5 | IIe | SSC | Init + dial + `CONNECT` + online | pass | 2026-09-19 | “SSC + modem + IIe seems ok” |
| M6 | IIgs | SCC modem | `+++` → command mode, CD stays up | pass | | HUD should stay `CD+`, mode `CMD`/`ESC` |
| M6a | either | Modem | After `+++`, `ATO` / `ATO0` → data + `CONNECT` | untested | | No socket → `NO CARRIER` |
| M7 | IIgs | SCC modem | `ATH` / `ATE0V1H` hang-up, CD drops | pass | | Guest should see loss / NO CARRIER |
| M8 | IIe | SSC | `+++` then `ATH`, CD drops | pass | | |
| M9 | IIgs | SCC printer (A) | Same as M1–M4 on port A | untested | | Confirm channel A path |
| M10 | either | Modem | Failed resolve / refused TCP → `NO CARRIER`, stay `CMD` | pass | | |
| M11 | either | Modem | Remote closes socket → CD−, `NO CARRIER` | pass | | Issue #179 analog for TCP |
| M12 | either | Modem | Inbound telnet :6502 → guest `RING`, HUD `RNG CD-` | partial | 2026-09-19 | IIgs + GBBS Pro answered on its own, so it saw RING; HUD not sampled |
| M13 | either | Modem | `ATA` / `ATA0` while ringing → `CONNECT`, HUD `ONL CD+` | pass | 2026-09-19 | GBBS logon banner + `Account Number` prompt reached the caller |
| M14 | either | Modem | `ATH` while ringing → pending drop, `OK`, `CMD CD-` | untested | | No `NO CARRIER` |
| M15 | either | Modem | Second inbound while ringing/online refused | untested | | First call unaffected |
| M16 | either | Modem | `ATV0` then `AT` → `0`; `ATV1` restores `OK` | untested | | RING=`2`; CONNECT 9600=`12` |
| M17 | either | Modem | `ATS0=1` inbound → RING then CONNECT (no ATA) | untested | | `ATS0=0` still needs ATA |
| M18 | IIgs | SCC modem | Inbound `telnet host 6502`, no client tweaks → character-at-a-time, no local echo | pass | 2026-09-19 | GBBS echoed each digit once at `Account Number` |
| M19 | either | Modem | Caller refusing every option → `CR NUL` counts as one Return, output gets `CR LF` | pass | 2026-09-19 | Raw socket client answering `DONT`/`WONT` to all |
| M20 | either | Modem | Binary agreed → no line-end rewriting either way | pass | 2026-09-19 | GBBS's `ATA` arrives as bare `CR`; needed for ZMODEM |
| M20a | either | Modem | Actual ZMODEM up/download through an inbound caller | untested | | SyncTerm or `sz`/`rz` against GBBS transfer menu |
| M21 | either | Modem | Caller drops socket → hang-up, next call rings | pass | 2026-09-19 | Reconnect accepted within 1 s |

### Handshake levels (HUD + guest)

| # | Device | Chip | Expect | Result | Date | Notes |
|---|--------|------|--------|--------|------|-------|
| H1 | Modem, idle | SCC / 6551 | `CMD CD- CTS+ DSR+` | partial | 2026-09-19 | HUD shown during ProTERM work; guest DCD bit not dumped |
| H2 | Modem, TCP up | SCC / 6551 | `ONL CD+ CTS+ DSR+` | pass | 2026-09-19 | HUD on IIgs; IIe not separately logged |
| H3 | File / Clipboard / Echo | either | `CD+ CTS+ DSR+` always | untested | | Capture must not stall on handshake |
| H4 | Host serial, detached | either | all off | untested | | |
| H5 | Host serial, attached, CD off | either | `CD-` live | untested | | WiModem idle |
| H6 | Host serial, carrier up | either | `CD+` live | untested | | |

### Host serial (issue #179)

Warp6 + USB-UART + WiModem232: hang-up must appear in the guest (6551 `ST_DCD` / SCC RR0 DCD), same as KEGS.

| # | Platform | Port | Guest | Scenario | Result | Date | Notes |
|---|----------|------|-------|----------|--------|------|-------|
| S1 | IIe | SSC | Warp6 | WiModem connect to BBS | untested | | |
| S2 | IIe | SSC | Warp6 | Remote hang-up / CD loss | untested | | Original #179 |
| S3 | IIgs | SCC | Warp6 or ProTERM | Same as S1 | untested | | |
| S4 | IIgs | SCC | Warp6 or ProTERM | Same as S2 | untested | | |
| S5 | either | host serial | any | Unplug dongle: CD/CTS/DSR off, retry, no crash | untested | | |

### Next to fill

1. M6 / M6a: `+++` then `ATO` back to data; M7–M8 hang-up.
2. M11 TCP drop.
3. S1–S2 with a real WiModem (#179).
4. H3 capture attachments (guest firmware that waits for DSR/CTS).
5. A second terminal (Spectrum) on one ModemDevice cell.
6. M14–M15 inbound `ATH` while ringing / second-call reject.
7. M16 `ATV0` numeric results.
8. M17 `ATS0=n` auto-answer.
9. Outbound ProTERM re-check (M2 / M4) after the telnet role split.
10. `IAC SB` … `IAC SE` is still skipped by resetting to data state, so a subnegotiation payload would leak into the guest stream. Nothing we accept asks for one today.

## Related

- [Serial & Parallel Connections](SerialConnections.md)
- [Serial / Modem](Serial_Modem.md)
- [Super Serial Card](Cards_SuperSerial.md)
- [SSC.md](SSC.md)
- [SCC8530_Serial.md](SCC8530_Serial.md)
