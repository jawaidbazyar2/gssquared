"""Frame encode/decode roundtrips (no emulator required)."""

from gs2debug.frame import pack_frame, unpack_header
from gs2debug.types import HELLO, PING


def test_pack_unpack_empty():
    raw = pack_frame(PING, 2, b"")
    assert len(raw) == 12
    t, s, n = unpack_header(raw)
    assert (t, s, n) == (PING, 2, 0)


def test_pack_unpack_payload():
    payload = b"\x01\x00\x00\x00\x00\x00\x00\x00"
    raw = pack_frame(HELLO, 1, payload)
    t, s, n = unpack_header(raw)
    assert (t, s, n) == (HELLO, 1, 8)
    assert raw[12:] == payload
