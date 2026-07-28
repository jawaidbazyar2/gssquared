# Networking

Several forms of networking are contemplated:

* Uthernet II emulation
* Serial-to-telnet emulation
* FujiNet

## Uthernet II

Selectable slot card (`card = "uthernet2"`) on Apple IIe and Apple IIgs platforms (slots 1–7).

Emulates the W5100-based Uthernet II (register model aligned with AppleWin):

- **MACRAW / IPRAW:** guest ethernet frames go through [libslirp](https://gitlab.com/qemu-project/libslirp/) (user-space NAT). No pcap/npcap, no root, works on macOS / Linux / Windows.
- **TCP / UDP offload:** host BSD sockets (non-blocking), on a dedicated worker thread so the emulator never blocks on network I/O.
- Slirp virtual `/24` is chosen at init so it does **not** overlap any host IPv4 interface (prefers `10.0.2.0/24` when free). Guest DHCP (MACRAW stacks such as Marinetti / IP65) sees that subnet; DNS is slirp’s `vnameserver` (typically `.3`).

**Limits (slirp NAT):** no LAN broadcast / SMB discovery on the real network; guest is double-NAT’d relative to the host’s LAN. HTTP browsing works. FTP is mixed: **SAFE2** (latest) works in **PASV** mode; active FTP and some older clients still fail under double NAT. A future pcap backend could be added if needed.

libslirp is vendored under `vendored/libslirp` and built with CMake plus a small in-tree `glib_compat` shim (no system GLib dependency).

### Contiki (Enhanced IIe)

Contiki does **not** auto-detect the card. Stock `contiki.cfg` ships pointed at the old Uthernet (`cs8900a.eth` at a bogus `$0003`). On each Contiki disk, run **`ETHCONFI.SYSTEM`** once:

1. Choose **Uthernet II** (driver `w5100.eth`, I/O base `$C0x4` where `x = 8 + slot`).
2. Choose the **same slot** as in your `.gs2` config (e.g. slot 3 → `$C0B4`).

Then run **`IPCONFIG.SYSTEM`** (manual IPs or DHCP). Contiki probes the W5100 Retry Time Register (`$0017` = `$07D0`); a slot mismatch shows as `w5100.eth: No hardware`.

With DHCP, Contiki should receive an address from slirp’s pool (typically starting at `.15` on the auto-picked `/24`, gateway `.2`, DNS `.3`).

## Serial-to-telnet

Emulates two pieces:

1. **Super Serial Card** — selectable slot card (`card = "super_serial"`). See [SSC.md](SSC.md).
2. **Hayes-compatible modem** as a telnet client — the same `ModemDevice` used by IIgs SCC port B. See [Serial_Modem.md](Serial_Modem.md).

Example:

```toml
[[cards]]
slot = 2
card = "super_serial"
```

v1 attaches a ModemDevice by default on native builds. `[[connections]]` with `slot` is reserved for choosing file/modem/echo later (not applied at runtime yet).


## FujiNet

ok there's a third one. FujiNet seems to be mostly organized around providing access to TNFS (trivial network file system) stores for disk images; 
There seems to be little need to do this, since the emulator is running on a machine that can access any network file system transparently.

There is also support for operating as a "character device", which can emulate a printer, or a modem. What Apple II software supports the SmartPort character interface?
Apparently it has a built-in terminal emulator program.
the emulated modem is hayes-compatible ATDT etc.

It may also offer a generic TCP/IP interface for certain apps. telnet and ssh. using program CATER. But other internet applications are supported.

There is a port of fujinet's firmware called fujinet-pc that works on mac/linux/pc: https://github.com/a8jan/fujinet-pc . This seems to run as a separate process on your local computer, then your emulator can connect to it.

"FujiNet utilizes a protocol called NetSIO to bridge between an emulator (like atari800) and the FujiNet device over a network connection. "

Now this sounds like you can use a real FujiNet device over ethernet using NetSIO. Or, alternatively, use FujiNet-PC in lieu of a physical device.

So in theory what could be done here is: implement SmartPort devices that talk the NetSIO protocol to a collocated instance of FujiNet-PC (or, to one you run separately on your computer). I like the collocated idea tho. This could be a lot of bang for the buck.

[ ] Control-Shift-2 and Control-Shift-6 don't seem to do the right stuff - should create an ASCII 0x00 and ??? needed for Lode Runner cheat.

## SMB FST

There is a new SMB FST released in the last 2 years. With an Uthernet, you can connect directly to SMB servers on Mac, Windows, Linux.

This is likely the way to go, because this is what most people actually use in their homes today.

## AppleTalk

I have been having a serious think about how to access networks from an emulated GS.

Of course, the first layer is a modem.

The second layer would be direct ethernet, simulating an Uthernet II card (seems to be the most popular). You could then use the SMB FST with this.

A third approach could be, simulate connection to an AppleTalk network. There were two methods for this:
* EtherTalk - appletalk directly over Ethernet network
* AppleTalk-in-IP (IPTalk) - appletalk packets encapsulated in IP packets, allowing routing across internetworks.

Linux Netatalk server can work over either.

"For modern MacOS connectivity, Netatalk is typically used over TCP/IP rather than pure EtherTalk." Huh.

Honestly, the model that makes the most sense is the SMB FST (see above).

