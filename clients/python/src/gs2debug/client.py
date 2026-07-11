"""Synchronous Unix-domain client for the GSSquared debug protocol."""

from __future__ import annotations

import socket
import struct
import time
from collections.abc import Callable
from dataclasses import dataclass

from .errors import ProtocolError
from .frame import HEADER_SIZE, Frame, pack_frame, unpack_header
from .keys import KMOD_LSHIFT, SCANCODE_LSHIFT, ascii_to_key
from .types import (
    ERROR,
    EVENT,
    GET_STATUS,
    HELLO,
    KEYEVENT,
    PING,
    PROTOCOL_VERSION,
    READMEM,
    WRITEMEM,
)


@dataclass(frozen=True)
class HelloInfo:
    version: int
    flags: int
    max_payload: int


EventHandler = Callable[[int, int, bytes], None]


class Client:
    def __init__(self) -> None:
        self._sock: socket.socket | None = None
        self._next_seq = 1
        self._handshaked = False
        self._event_handler: EventHandler | None = None
        self._busy = False

    def connect(self, path: str) -> None:
        if self._sock is not None:
            raise RuntimeError("already connected")
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(path)
        self._sock = sock
        self._handshaked = False
        self._next_seq = 1

    def close(self) -> None:
        if self._sock is not None:
            try:
                self._sock.close()
            finally:
                self._sock = None
                self._handshaked = False

    def on_event(self, handler: EventHandler | None) -> None:
        self._event_handler = handler

    def hello(self, version: int = PROTOCOL_VERSION) -> HelloInfo:
        payload = struct.pack("<II", version, 0)
        reply = self.request(HELLO, payload)
        if len(reply) != 12:
            raise ProtocolError(0, f"HELLO reply length {len(reply)}, expected 12")
        ver, flags, max_payload = struct.unpack("<III", reply)
        self._handshaked = True
        return HelloInfo(version=ver, flags=flags, max_payload=max_payload)

    def ping(self) -> None:
        if not self._handshaked:
            raise RuntimeError("hello() required before ping()")
        reply = self.request(PING, b"")
        if reply:
            raise ProtocolError(0, f"PING reply not empty ({len(reply)} bytes)")

    def get_status(self) -> int:
        """Return execution_mode: 0=NORMAL, 1=STEP_INTO, 2=PAUSED."""
        if not self._handshaked:
            raise RuntimeError("hello() required before get_status()")
        reply = self.request(GET_STATUS, b"")
        if len(reply) != 4:
            raise ProtocolError(0, f"GET_STATUS reply length {len(reply)}, expected 4")
        return struct.unpack("<I", reply)[0]

    def read_mem(self, domain: int, address: int, length: int) -> bytes:
        """Send READMEM; return `length` bytes from domain/address (MAIN=0 implemented)."""
        if not self._handshaked:
            raise RuntimeError("hello() required before read_mem()")
        if length < 0:
            raise ValueError("length must be non-negative")
        payload = struct.pack("<III", domain, address, length)
        reply = self.request(READMEM, payload)
        if len(reply) != length:
            raise ProtocolError(0, f"READMEM reply length {len(reply)}, expected {length}")
        return reply

    def write_mem(self, domain: int, address: int, data: bytes) -> None:
        """Send WRITEMEM; poke `data` at domain/address (MAIN=0 implemented)."""
        if not self._handshaked:
            raise RuntimeError("hello() required before write_mem()")
        if not data:
            raise ValueError("data must be non-empty")
        payload = struct.pack("<III", domain, address, len(data)) + data
        reply = self.request(WRITEMEM, payload)
        if reply:
            raise ProtocolError(0, f"WRITEMEM reply not empty ({len(reply)} bytes)")

    def key_event(self, down: bool, scancode: int, mod: int = 0) -> None:
        """Send KEYEVENT (one SDL key down or up)."""
        if not self._handshaked:
            raise RuntimeError("hello() required before key_event()")
        payload = struct.pack("<III", 1 if down else 0, scancode, mod)
        reply = self.request(KEYEVENT, payload)
        if reply:
            raise ProtocolError(0, f"KEYEVENT reply not empty ({len(reply)} bytes)")

    def key_down(self, scancode: int, mod: int = 0) -> None:
        self.key_event(True, scancode, mod)

    def key_up(self, scancode: int, mod: int = 0) -> None:
        self.key_event(False, scancode, mod)

    def tap_key(self, scancode: int, mod: int = 0) -> None:
        """Send keydown then keyup with the same scancode/mod."""
        self.key_down(scancode, mod)
        self.key_up(scancode, mod)

    def type_text(self, text: str, *, delay_s: float = 0.05) -> None:
        """Type printable ASCII (US layout). Newline becomes Return; shift held per glyph."""
        if not self._handshaked:
            raise RuntimeError("hello() required before type_text()")
        for ch in text:
            scancode, needs_shift = ascii_to_key(ch)
            if needs_shift:
                self.key_down(SCANCODE_LSHIFT, KMOD_LSHIFT)
                self.tap_key(scancode, KMOD_LSHIFT)
                self.key_up(SCANCODE_LSHIFT, 0)
            else:
                self.tap_key(scancode, 0)
            if delay_s > 0:
                time.sleep(delay_s)

    def request(
        self,
        type_word: int,
        payload: bytes = b"",
        *,
        timeout: float | None = None,
    ) -> bytes:
        sock = self._require_sock()
        if self._busy:
            raise RuntimeError("only one outstanding request allowed")
        seq = self._alloc_seq()
        self._busy = True
        try:
            self._send_all(pack_frame(type_word, seq, payload))
            deadline = None if timeout is None else time.monotonic() + timeout
            while True:
                remaining = None
                if deadline is not None:
                    remaining = deadline - time.monotonic()
                    if remaining <= 0:
                        raise TimeoutError("request timed out")
                frame = self._recv_frame(timeout=remaining)
                if frame.type == EVENT:
                    self._dispatch_event(frame)
                    continue
                if frame.seq != seq:
                    raise ProtocolError(0, f"seq mismatch: got {frame.seq}, want {seq}")
                if frame.type == ERROR:
                    code, message = self._parse_error(frame.payload)
                    raise ProtocolError(code, message)
                if frame.type != type_word:
                    raise ProtocolError(
                        0,
                        f"type mismatch: got 0x{frame.type:08x}, want 0x{type_word:08x}",
                    )
                return frame.payload
        finally:
            self._busy = False
            sock.settimeout(None)

    def _alloc_seq(self) -> int:
        seq = self._next_seq
        self._next_seq = seq + 1 if seq < 0xFFFFFFFF else 1
        return seq

    def _require_sock(self) -> socket.socket:
        if self._sock is None:
            raise RuntimeError("not connected")
        return self._sock

    def _send_all(self, data: bytes) -> None:
        sock = self._require_sock()
        view = memoryview(data)
        while view:
            n = sock.send(view)
            if n == 0:
                raise ConnectionError("socket closed during send")
            view = view[n:]

    def _recv_exact(self, n: int, timeout: float | None) -> bytes:
        sock = self._require_sock()
        if timeout is not None:
            sock.settimeout(timeout)
        buf = bytearray()
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionError("socket closed during recv")
            buf.extend(chunk)
        return bytes(buf)

    def _recv_frame(self, timeout: float | None) -> Frame:
        header = self._recv_exact(HEADER_SIZE, timeout)
        type_word, seq, length = unpack_header(header)
        payload = b""
        if length:
            # Remaining time after header read is approximate; pass timeout again.
            payload = self._recv_exact(length, timeout)
        return Frame(type=type_word, seq=seq, payload=payload)

    def _dispatch_event(self, frame: Frame) -> None:
        if len(frame.payload) < 4:
            return
        event_id = struct.unpack_from("<I", frame.payload, 0)[0]
        data = frame.payload[4:]
        if self._event_handler is not None:
            self._event_handler(event_id, frame.seq, data)

    @staticmethod
    def _parse_error(payload: bytes) -> tuple[int, str]:
        if len(payload) < 4:
            return 0, ""
        code = struct.unpack_from("<I", payload, 0)[0]
        message = payload[4:].decode("utf-8", errors="replace")
        return code, message

    def __enter__(self) -> Client:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()
