#!/usr/bin/env python3
"""ThunderClock Plus smoke: ROM ID, TIME READ/SET, IRQ ack aliases.

Usage (from repo root):

  PYTHONPATH=clients/python/src python3 clients/python/examples/test_thunderclock.py

  GS2_TCP_DISK=/path/to/thunderclock1.dsk \\
    PYTHONPATH=clients/python/src python3 clients/python/examples/test_thunderclock.py

Launches ``./build/GSSquared --debug <sock> --no-quit-confirm assets/gs2/IIe_ThunderClock.gs2``.
Optional ``GS2_TCP_DISK`` mounts a Thunderware DOS 3.3 utilities image on slot 6
and boots far enough to catalog.
"""

from __future__ import annotations

import os
import struct
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from gs2debug import (
    KMOD_CTRL,
    KMOD_LCTRL,
    MEM_MAIN,
    MEDIA_OK,
    PLATFORM_APPLE_IIE_ENHANCED,
    REG_P,
    SCANCODE_F12,
    SCANCODE_LCTRL,
    Client,
    ProtocolError,
)

REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_GS2 = REPO_ROOT / "build" / "GSSquared"
GS2_CONFIG = REPO_ROOT / "assets" / "gs2" / "IIe_ThunderClock.gs2"
SLOT = 5
CREG = 0xC080 + (SLOT << 4)  # $C0D0; aliases $C0D0–$C0DF
CLK = 0x02
STB = 0x04
IRQEN = 0x40
IRQ_STATUS = 0x20
DATA_OUT = 0x80
CMD_HOLD = 0x00
CMD_SHIFT = 0x08
CMD_TIME_SET = 0x10
CMD_TIME_READ = 0x18
CMD_TP_64 = 0x20

P_I = 0x04


def find_gs2() -> Path:
    env = os.environ.get("GS2_BIN")
    path = Path(env) if env else DEFAULT_GS2
    if not path.is_file():
        raise FileNotFoundError(f"GSSquared binary not found: {path}")
    return path


def wait_for_socket(path: Path, timeout_s: float = 45.0) -> None:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if path.exists():
            try:
                with Client() as probe:
                    probe.connect(str(path))
                    probe.hello()
                return
            except (OSError, ProtocolError, RuntimeError):
                pass
        time.sleep(0.2)
    raise TimeoutError(f"debug socket not ready: {path}")


def wr(c: Client, value: int, addr: int = CREG) -> None:
    c.write_mem(MEM_MAIN, addr, bytes([value & 0xFF]))


def rd(c: Client, addr: int = CREG) -> int:
    return c.read_mem(MEM_MAIN, addr, 1)[0]


def strobe(c: Client, cmd: int) -> None:
    wr(c, cmd)
    wr(c, cmd | STB)
    wr(c, cmd)


def read_nibble(c: Client) -> int:
    nibble = 0
    for i in range(4):
        bit = 1 if (rd(c) & DATA_OUT) else 0
        nibble |= bit << i
        wr(c, CMD_SHIFT)
        wr(c, CMD_SHIFT | CLK)
        wr(c, CMD_SHIFT)
    return nibble


def read_time(c: Client) -> list[int]:
    strobe(c, CMD_TIME_READ)
    strobe(c, CMD_SHIFT)
    return [read_nibble(c) for _ in range(10)]


def write_time(c: Client, nibbles: list[int]) -> None:
    strobe(c, CMD_SHIFT)
    for nib in nibbles:
        for i in range(4):
            di = 1 if (nib & (1 << i)) else 0
            wr(c, CMD_SHIFT | di)
            wr(c, CMD_SHIFT | di | CLK)
            wr(c, CMD_SHIFT | di)
    strobe(c, CMD_TIME_SET)


def unpack_trace_p(regs: bytes) -> int:
    return regs[13]


def sei(c: Client) -> None:
    p = unpack_trace_p(c.get_regs())
    c.set_regs(REG_P, p=p | P_I)


def decode_text40_row(row: bytes) -> str:
    return "".join(
        chr(b & 0x7F) if 0x20 <= (b & 0x7F) <= 0x7E else " " for b in row
    )


_TEXT_LINE_BASES = (
    0x400, 0x480, 0x500, 0x580, 0x600, 0x680, 0x700, 0x780,
    0x428, 0x4A8, 0x528, 0x5A8, 0x628, 0x6A8, 0x728, 0x7A8,
    0x450, 0x4D0, 0x550, 0x5D0, 0x650, 0x6D0, 0x750, 0x7D0,
)


def read_screen_text(c: Client) -> str:
    return "\n".join(
        decode_text40_row(c.read_mem(MEM_MAIN, base, 0x28)) for base in _TEXT_LINE_BASES
    )


def check_rom(c: Client) -> None:
    rom = c.read_mem(MEM_MAIN, 0xC500, 8)
    assert rom[0] == 0x08, rom
    assert rom[2] == 0x28, rom
    assert rom[4] == 0x58, rom
    assert rom[6] == 0x70, rom
    print(f"  slot ROM signature ok ({rom[:8].hex()})")


def check_time_read(c: Client) -> list[int]:
    # Stream is seconds-ones first (Docs/ThunderClock.md).
    nibs = read_time(c)
    sec_o, sec_t, min_o, min_t, hr_o, hr_t, date_o, date_t, dow, month = nibs
    date = date_t * 10 + date_o
    hour = hr_t * 10 + hr_o
    minute = min_t * 10 + min_o
    second = sec_t * 10 + sec_o
    assert 1 <= month <= 12, nibs
    assert 0 <= dow <= 6, nibs
    assert 1 <= date <= 31, nibs
    assert 0 <= hour <= 23, nibs
    assert 0 <= minute <= 59, nibs
    assert 0 <= second <= 59, nibs
    print(
        f"  TIME READ ok  {month:02d} dow={dow} {date:02d} {hour:02d}:{minute:02d}:{second:02d}"
    )
    return nibs


def check_time_set(c: Client) -> None:
    # 15 Mar, Wednesday, 12:34:56 — seconds-ones first, month last.
    want = [6, 5, 4, 3, 2, 1, 5, 1, 3, 3]
    write_time(c, want)
    got = read_time(c)
    assert got == want, (want, got)
    print(f"  TIME SET/READ ok  {got}")


def check_irq(c: Client) -> None:
    sei(c)
    wr(c, 0)
    wr(c, CMD_TP_64)
    wr(c, CMD_TP_64 | STB)
    wr(c, CMD_TP_64)
    wr(c, IRQEN)
    c.continue_()
    time.sleep(0.08)
    c.pause()
    try:
        c.wait_stopped(timeout=2.0)
    except TimeoutError:
        pass
    v8 = rd(c, CREG + 8)
    assert v8 & IRQ_STATUS, f"$C0D8 expected IRQ status, got ${v8:02X}"
    v0 = rd(c, CREG)
    assert (v0 & IRQ_STATUS) == 0, f"$C0D0 should have cleared IRQ, got ${v0:02X}"
    v0b = rd(c, CREG)
    assert (v0b & IRQ_STATUS) == 0, f"second $C0D0 still set ${v0b:02X}"
    wr(c, 0)
    print(f"  IRQ latch/ack ok  $C0D8=${v8:02X} $C0D0=${v0:02X}")


def control_reset(c: Client) -> None:
    c.key_down(SCANCODE_LCTRL, KMOD_LCTRL)
    time.sleep(0.05)
    c.key_down(SCANCODE_F12, KMOD_CTRL)
    time.sleep(0.1)
    c.key_up(SCANCODE_F12, KMOD_CTRL)
    time.sleep(0.05)
    c.key_up(SCANCODE_LCTRL, 0)


def wait_for_screen(c: Client, needles: tuple[str, ...], timeout_s: float) -> str:
    deadline = time.monotonic() + timeout_s
    last = ""
    while time.monotonic() < deadline:
        last = read_screen_text(c)
        upper = last.upper()
        if any(n.upper() in upper for n in needles):
            return last
        time.sleep(0.4)
    raise TimeoutError(f"timed out waiting for {needles!r}; screen=\n{last}")


def print_screen(c: Client, label: str) -> str:
    screen = read_screen_text(c)
    print(f"  {label}:")
    for line in screen.splitlines():
        if line.strip():
            print(f"    {line}")
    return screen


def check_utils_disk(c: Client, disk: Path) -> None:
    st = c.mount(6, 0, str(disk.resolve()))
    assert st == MEDIA_OK, st
    print(f"  mounted {disk}")
    c.reset(cold_start=False)
    # HELLO -> LASTBOOT -> INTRO -> CLOCK; Disk II at 1 MHz needs a long spin-up.
    print("  waiting for DOS 3.3 HELLO/CLOCK boot...")
    time.sleep(20.0)
    screen = wait_for_screen(
        c,
        ("THUNDERCLOCK IN SLOT", "NO THUNDERCLOCK FOUND", "THUNDERCLOCK PLUS",
         "CURRENT THUNDERCLOCK", "LAST BOOTED"),
        40.0,
    )
    print_screen(c, "DOS boot")
    upper = screen.upper()
    assert "NO THUNDERCLOCK FOUND" not in upper, "LASTBOOT did not find the card"
    if "THUNDERCLOCK IN SLOT" in upper or "THUNDERCLOCK PLUS" in upper:
        print("  LASTBOOT/INTRO found ThunderClock")

    control_reset(c)
    print("  waiting for DOS prompt after Control-Reset...")
    time.sleep(8.0)
    c.type_text("RUN TEST\n", delay_s=0.08)
    wait_for_screen(c, ("ENTER THUNDERCLOCK SLOT",), 30.0)
    c.type_text("5\n", delay_s=0.1)
    screen = wait_for_screen(
        c,
        ("APPEARS TO BE OK", "NOT OPERATING", "HAS A PROBLEM"),
        40.0,
    )
    print_screen(c, "TEST")
    upper = screen.upper()
    assert "NOT OPERATING" not in upper, "TEST reported card failure"
    assert "HAS A PROBLEM" not in upper, "TEST reported slot/problem"
    assert "APPEARS TO BE OK" in upper, "TEST did not report success"
    print("  DOS TEST ok")


def check_prodos_disk(c: Client, disk: Path) -> None:
    st = c.mount(6, 0, str(disk.resolve()))
    assert st == MEDIA_OK, st
    print(f"  mounted {disk}")
    c.reset(cold_start=False)
    print("  waiting for ProDOS boot...")
    time.sleep(25.0)
    print_screen(c, "ProDOS boot")
    machid = c.read_mem(MEM_MAIN, 0xBF98, 1)[0]
    bf06 = c.read_mem(MEM_MAIN, 0xBF06, 1)[0]
    print(f"  MACHID $BF98=${machid:02X}  $BF06=${bf06:02X}")
    assert machid & 0x01, f"ProDOS did not set MACHID clock bit ($BF98=${machid:02X})"
    assert bf06 == 0x4C, f"ProDOS clock vector not JMP ($BF06=${bf06:02X})"
    print("  ProDOS ThunderClock recognition ok")


def main() -> int:
    if not GS2_CONFIG.is_file():
        raise FileNotFoundError(f"missing config: {GS2_CONFIG}")

    sock = Path(f"/tmp/gs2-thunderclock-{os.getpid()}.sock")
    if sock.exists():
        sock.unlink()

    disk_env = os.environ.get("GS2_TCP_DISK", "")
    default_dos = Path("/Users/bazyar/src/AppleIIDisks/thunderclock1.dsk")
    default_prodos = Path("/Users/bazyar/src/AppleIIDisks/thunderclock2.dsk")
    dos_disk = Path(disk_env) if disk_env else (default_dos if default_dos.is_file() else None)
    if disk_env and dos_disk is not None and not dos_disk.is_file():
        raise FileNotFoundError(f"GS2_TCP_DISK not found: {dos_disk}")
    prodos_disk = default_prodos if default_prodos.is_file() else None

    gs2 = find_gs2()
    proc = subprocess.Popen(
        [
            str(gs2),
            "--debug",
            str(sock),
            "--no-quit-confirm",
            str(GS2_CONFIG),
        ],
        cwd=str(REPO_ROOT),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    try:
        wait_for_socket(sock)
        with Client() as c:
            c.connect(str(sock))
            c.hello()
            st = c.get_status()
            assert st.platform_id == PLATFORM_APPLE_IIE_ENHANCED, st

            time.sleep(1.5)
            c.pause()
            try:
                c.wait_stopped(timeout=2.0)
            except TimeoutError:
                pass

            check_rom(c)
            check_time_read(c)
            check_time_set(c)
            check_irq(c)

            if dos_disk is not None:
                c.continue_()
                check_utils_disk(c, dos_disk)
            if prodos_disk is not None:
                check_prodos_disk(c, prodos_disk)

            c.quit()
    finally:
        try:
            proc.wait(timeout=8)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=2)

    print("all ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
