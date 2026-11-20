# Net Printer — implementation spec

A serial attachment that copies the guest UART byte stream onto a raw TCP connection. User guide updates (Serial & Parallel Connections, config TOML, Super Serial) land with the implementation. Handshake model: [SerialPortSpec.md](SerialPortSpec.md). The local ImageWriter path this timeout matches: [PrinterEmulation.md](PrinterEmulation.md).

## Goal

On the Super Serial Card and both IIgs SCC channels, offer **Net Printer**. The user types a host (DNS name or IP address) and a TCP port. The emulator opens a TCP stream when a print job starts and closes it after the job goes idle.

The bytes on the wire are the UART bytes. There is no Telnet, no HTTP, no JetDirect header, and no `.gs2print` ticket. This is the client side of a raw printer port (often 9100) or of the printer app’s server mode.

## Where it appears

| Port | Offered |
|------|---------|
| Super Serial Card | yes |
| IIgs SCC channel A (firmware slot 1) and B (slot 2) | yes |
| Parallel card | no |
| Browser build | no |

Parallel stays file and clipboard. The browser build has no raw TCP client; a `netprinter` row there is treated like `serial` on the web (the port stays unattached).

## Config

`device` is `netprinter`. The Control Panel and the config editor label that choice **Net Printer**.

Host and port are their own keys. They are not stored in `path` (that key is a filesystem path, except for a host UART name) and not in `remote_url` (reserved for a future modem URL).

```toml
[[connections]]
slot = 1
device = "netprinter"
host = "printer.local"
port = 9100
```

| Key | Type | Required | Meaning |
|-----|------|----------|---------|
| `device` | string | yes | `netprinter` |
| `host` | string | yes | DNS name, IPv4 literal, or IPv6 literal. Not resolved when the config is saved. |
| `port` | integer | yes | TCP port, 1–65535 |

A row missing `host` or `port`, or with a port outside that range, fails to load. The editor refuses to save the attachment until both fields are filled. The port field is prefilled with **9100**; the user can change it.

`host` is stored as typed. It is not rewritten relative to the config file.

## UI

Control Panel (F4) and the config editor both list **Net Printer** on SSC and SCC port buttons, next to File, Printer, Clipboard, and Modem.

Choosing it opens two fields, Host and Port, and a confirm button. Confirm on the Control Panel attaches immediately. Confirm in the editor writes the draft; **Save** writes the `.gs2` file.

The port button bracket is **Net**. The current endpoint is a line under that button (`printer.local:9100`). An IPv6 literal is shown in brackets (`[fe80::1]:9100`). The line is absent until a host is set.

The HUD status line (the same one host-serial and modem already fill) reads `Net Printer host:port` plus `up` while the socket is open and `idle` while it is not, then the usual `CD+ CTS+ DSR+`.

## Device

`NetPrinterDevice` is a `SerialDevice`. `Connections` owns it, the same way it owns `FileDevice` and `ModemDevice`. The worker thread is started when the attachment is selected and stopped when the user picks something else or the machine shuts down. The TCP socket is not opened at attach time.

Guest baud, parity, and stop bits stay on the 6551 or SCC. They are not sent to the peer and they do not pace the socket. The UART already delivered the bytes.

### Handshake

CD, CTS, and DSR stay asserted for the whole time the attachment is selected, including while the socket is closed. That is the File / Printer policy. Firmware that waits for CTS can then emit the first byte, and that byte is what opens the socket.

On the SSC, selecting Net Printer sets DIPSW1 mode bits to SIC P8, the same change the Printer attachment makes, so firmware does not send ETX or wait for ACK. A raw TCP peer will not answer that handshake. Leaving the port restores the card default.

SCC has no DIP switch to rewrite. Asserted CTS is enough for the firmware printer port.

### Job open and close

Same rule as the Printer attachment’s idle timer (`FileDevice` printer path, 60 seconds):

| Event | Socket |
|-------|--------|
| First `MESSAGE_DATA` while closed | Resolve `host`, connect, then write that byte |
| Further `MESSAGE_DATA` while open | Write the byte, reset the idle timer |
| 60 seconds with no guest byte | Close |
| `MESSAGE_CLOSE` (reset) | Close |
| `MESSAGE_SHUTDOWN` (detach or quit) | Close |
| Peer closes | Close |
| Next guest byte after a close | New connection |

The 60 seconds is the printer idle interval, not the 10 second File / Clipboard interval. One shared constant with the Printer path, so the two cannot drift.

There is no capture file. A user who wants a dump uses File or Printer.

### Connect

Resolve and connect on the worker, at the start of each job, so a DNS change is picked up on the next job.

The connect is non-blocking and polled from `device_loop`, with a **5 second** limit (the same limit the printer sidecar connect uses). The loop still drains `MESSAGE_SHUTDOWN` and `MESSAGE_CLOSE` during the wait. A blocking connect would pin the worker and ignore detach.

One attempt per job. If resolve or connect fails, show one OSD toast naming the host and port, drop guest bytes until the job ends (idle, reset, or detach), and let the next job try again. Do not retry on every byte.

Bytes that arrive during the connect wait are held and written in order once the socket is up. If the attempt fails, that hold is discarded with the rest of the job.

### On the wire

TCP stream, full duplex, one byte in, one byte out.

- Guest TX is written as the UART produced it, high bit included.
- Peer bytes are copied into `q_dev` and become serial RX. An ImageWriter `ESC ?` answer, or any other status the peer sends, has a path back. If `q_dev` is full, leave the bytes in the socket and read them on a later poll.
- A send that would block stays queued on the worker and is retried on the next loop. The worker does not sit inside a blocking `send`.
- `MESSAGE_LINE` is ignored. Break is ignored.

### Toasts

| Situation | Message |
|-----------|---------|
| Resolve or connect failed | `Net Printer: could not reach <host>:<port>` |
| Peer closed before the idle timeout | `Net Printer: <host>:<port> closed` |

One toast per failed job. A clean idle close is silent.

## Loader and editor

- `connection_device_type_t::NET_PRINTER`, name `netprinter`.
- Allowed on `connection_port_kind_t::SERIAL` on native builds. Refused on parallel and on the web.
- `validate_connections` accepts `netprinter` only with a non-empty `host` and a port in 1–65535.
- A `netprinter` row on a parallel port is refused at attach; the editor rewrites it away the same way it rewrites a `printer` row on parallel.
- `connection_config_t` gains `host` (`string`) and `port` (`int`, 0 means unset). Load and save round-trip both keys. `port = 0` or a missing port is the unset state and fails validation for this device.

## Out of scope

- Listening for inbound connections. Net Printer dials out.
- ImageWriter emulation, PDF, or a `.gs2print` ticket. That remains the Printer attachment.
- Telnet option negotiation. That remains the Modem attachment.
- Applying guest baud to the TCP connection.
- Parallel-card jobs.
