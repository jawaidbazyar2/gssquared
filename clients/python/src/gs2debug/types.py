"""Protocol type constants and helpers. See Docs/DebugProtocol.md."""

from __future__ import annotations

HELLO = 0x00000001
PING = 0x00000002
ERROR = 0x00000003
EVENT = 0x00000004
GET_STATUS = 0x00000101
RESET = 0x00000102
READMEM = 0x00000301
WRITEMEM = 0x00000302
KEYEVENT = 0x00000501

# READMEM / WRITEMEM domains (Docs/DebugProtocol.md)
MEM_MAIN = 0
MEM_MEGAII = 1
MEM_ENSONIQ = 2
MEM_ADBMICRO = 3

FLAGS_MASK = 0xFF000000
MAIN_MASK = 0x00FFFF00
SUB_MASK = 0x000000FF
TYPE_MASK = 0x00FFFFFF

MAX_PAYLOAD = 0x00100000
PROTOCOL_VERSION = 1

E_UNKNOWN_TYPE = 1
E_BAD_LENGTH = 2
E_BAD_VERSION = 3
E_NOT_HANDSHAKED = 4
E_BUSY = 5
E_INTERNAL = 6


def make_type(main: int, sub: int, flags: int = 0) -> int:
    return ((flags & 0xFF) << 24) | ((main & 0xFFFF) << 8) | (sub & 0xFF)


def type_main(type_word: int) -> int:
    return (type_word & MAIN_MASK) >> 8


def type_sub(type_word: int) -> int:
    return type_word & SUB_MASK


def type_flags(type_word: int) -> int:
    return (type_word & FLAGS_MASK) >> 24
