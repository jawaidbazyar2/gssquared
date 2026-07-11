"""Frame pack/unpack for the 12-byte LE header + payload."""

from __future__ import annotations

import struct
from dataclasses import dataclass

from .types import MAX_PAYLOAD

HEADER_STRUCT = struct.Struct("<III")  # type, seq, length
HEADER_SIZE = HEADER_STRUCT.size


@dataclass(frozen=True)
class Frame:
    type: int
    seq: int
    payload: bytes


def pack_frame(type_word: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError(f"payload length {len(payload)} exceeds max {MAX_PAYLOAD}")
    return HEADER_STRUCT.pack(type_word, seq, len(payload)) + payload


def unpack_header(data: bytes) -> tuple[int, int, int]:
    if len(data) < HEADER_SIZE:
        raise ValueError("short header")
    type_word, seq, length = HEADER_STRUCT.unpack(data[:HEADER_SIZE])
    if length > MAX_PAYLOAD:
        raise ValueError(f"frame length {length} exceeds max {MAX_PAYLOAD}")
    return type_word, seq, length
