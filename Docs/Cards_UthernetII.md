# Uthernet II

Uthernet II is an Ethernet card for the Apple IIe and Apple IIgs. In GSSquared it gives emulated software a working TCP/IP path to the internet without installing drivers or virtual adapters on your host computer.

## What it does

- Emulates a W5100-based Uthernet II in an expansion slot.
- Uses user-space networking (slirp): HTTP, DHCP, and many TCP apps work out of the box on macOS, Windows, and Linux.
- No root/admin privileges, TAP device, or WinPcap/Npcap install required for normal use.

**Limits:** The guest is behind NAT. Local LAN broadcast / SMB discovery of machines on your real network generally will not work. Passive browsing and many TCP clients are fine; active-mode FTP is unreliable under double NAT.

## How to enable it

### In the config editor

1. At System Select, open **+** or **Edit…**.
2. Choose a **platform**: Enhanced //e or Apple IIgs (ROM 01 or ROM 03).
3. Click an empty slot (1–7) and pick **Uthernet II**.
4. **Save** the `.gs2` config, then launch it.

### In a `.gs2` file

```toml
[[cards]]
slot = 3
card = "uthernet2"
```

You can install more than one Uthernet II in different slots if needed.

## Using it from Apple II software

Configure the guest stack for **Uthernet II** (W5100), not the older Uthernet (CS8900). The I/O base is `$C0x4` where `x = 8 + slot` (slot 3 → `$C0B4`).

### Contiki (Enhanced IIe)

Contiki does not auto-detect the card. On each Contiki disk:

1. Run **`ETHCONFI.SYSTEM`**, choose **Uthernet II**, and set the **same slot** as in your config.
2. Run **`IPCONFIG.SYSTEM`** (DHCP or manual).

A slot mismatch usually shows as `w5100.eth: No hardware`.

### GS/OS / Marinetti

Install and configure Marinetti (or another IIgs TCP stack) for Uthernet II in the slot you chose. With DHCP, the guest typically receives an address on slirp’s private `/24` (gateway often `.2`, DNS `.3`).

## Related

- Serial modem “dial-up” style access: [Serial / Modem](Serial_Modem.md)
- Hand-editing configs: [Writing Config Files Manually](ConfigFiles.md)
