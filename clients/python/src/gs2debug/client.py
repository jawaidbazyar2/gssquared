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
    BP_ACCESS_NONE,
    BP_ACCESS_R,
    BP_ACCESS_RW,
    BP_ACCESS_W,
    BP_CLEAR,
    BP_CLEAR_ALL,
    BP_ENABLE,
    BP_FLAG_ENABLED,
    BP_KIND_DATA,
    BP_KIND_EXEC,
    BP_KIND_IO,
    BP_LIST,
    BP_SET,
    CONTINUE,
    ERROR,
    EVENT,
    EVT_RUN_STATE,
    EVT_STOPPED,
    EXEC_NORMAL,
    EXEC_PAUSED,
    EXEC_STEP_INTO,
    GET_STATUS,
    GET_TRACE,
    HELLO,
    KEYEVENT,
    PAUSE,
    PING,
    PROTOCOL_VERSION,
    QUIT,
    READMEM,
    RESET,
    STEP_INTO,
    STOP_BP_DATA,
    STOP_BP_EXEC,
    STOP_BP_IO,
    STOP_PAUSE,
    STOP_STEP,
    WRITEMEM,
)


@dataclass(frozen=True)
class HelloInfo:
    version: int
    flags: int
    max_payload: int


@dataclass(frozen=True)
class StatusInfo:
    execution_mode: int
    platform_id: int


@dataclass(frozen=True)
class BpInfo:
    id: int
    hit_count: int
    kind: int
    flags: int
    access: int
    domain: int
    address: int
    length: int
    addr_mask: int
    data_value: int
    data_mask: int
    ignore_count: int


@dataclass(frozen=True)
class StoppedEvent:
    reason: int
    bp_id: int
    pc: int
    eaddr: int
    value: int
    access: int
    kind: int
    execution_mode: int
    trace: bytes


@dataclass(frozen=True)
class TraceWindow:
    """GET_TRACE reply: ring size and packed 40-byte system_trace_entry_t blobs."""

    available: int
    entries: list[bytes]  # oldest → newest; each len == 40


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

    def quit(self) -> None:
        """Force-quit the emulator (skips QuitModal / dirty-disk prompts).

        Prefer this over killing the process; SIGTERM still becomes SDL_EVENT_QUIT
        and shows the confirm dialog unless GSSquared was started with
        ``--no-quit-confirm``.
        """
        if not self._handshaked:
            raise RuntimeError("hello() required before quit()")
        reply = self.request(QUIT, b"")
        if reply:
            raise ProtocolError(0, f"QUIT reply not empty ({len(reply)} bytes)")

    def get_status(self) -> StatusInfo:
        """Return execution_mode and platform_id (PlatformId_t / -p N)."""
        if not self._handshaked:
            raise RuntimeError("hello() required before get_status()")
        reply = self.request(GET_STATUS, b"")
        if len(reply) != 8:
            raise ProtocolError(0, f"GET_STATUS reply length {len(reply)}, expected 8")
        mode, platform_id = struct.unpack("<II", reply)
        return StatusInfo(execution_mode=mode, platform_id=platform_id)

    def reset(self, cold_start: bool = False) -> None:
        """Call computer_t::reset(cold_start) on the main thread."""
        if not self._handshaked:
            raise RuntimeError("hello() required before reset()")
        payload = struct.pack("<I", 1 if cold_start else 0)
        reply = self.request(RESET, payload)
        if reply:
            raise ProtocolError(0, f"RESET reply not empty ({len(reply)} bytes)")

    def pause(self) -> None:
        if not self._handshaked:
            raise RuntimeError("hello() required before pause()")
        reply = self.request(PAUSE, b"")
        if reply:
            raise ProtocolError(0, f"PAUSE reply not empty ({len(reply)} bytes)")

    def continue_(self) -> None:
        """Resume execution (CONTINUE)."""
        if not self._handshaked:
            raise RuntimeError("hello() required before continue_()")
        reply = self.request(CONTINUE, b"")
        if reply:
            raise ProtocolError(0, f"CONTINUE reply not empty ({len(reply)} bytes)")

    def step_into(self, count: int = 1) -> None:
        """Arm EXEC_STEP_INTO for `count` instructions (instructions_left).

        Reply is empty; when the batch finishes the emu emits EVT_STOPPED
        (STOP_STEP) with the post-instruction CPU trace. count must be >= 1.
        """
        if not self._handshaked:
            raise RuntimeError("hello() required before step_into()")
        if count < 1:
            raise ValueError("step_into count must be >= 1")
        reply = self.request(STEP_INTO, struct.pack("<I", count))
        if reply:
            raise ProtocolError(0, f"STEP_INTO reply not empty ({len(reply)} bytes)")

    def get_trace(self, ago: int = 0, count: int = 100) -> TraceWindow:
        """Read a window from the instruction trace ring buffer.

        ``ago`` is how many instructions before the newest completed entry
        to place the window's newest end (0 = most recent). ``count`` is how
        many records to return extending into the past (clamped by available).
        """
        if not self._handshaked:
            raise RuntimeError("hello() required before get_trace()")
        if ago < 0:
            raise ValueError("get_trace ago must be >= 0")
        if count < 1:
            raise ValueError("get_trace count must be >= 1")
        reply = self.request(GET_TRACE, struct.pack("<II", ago, count))
        if len(reply) < 8:
            raise ProtocolError(0, f"GET_TRACE reply too short ({len(reply)} bytes)")
        available, returned = struct.unpack_from("<II", reply, 0)
        expect = 8 + returned * 40
        if len(reply) != expect:
            raise ProtocolError(
                0, f"GET_TRACE reply length {len(reply)}, expected {expect}"
            )
        entries = [reply[8 + i * 40 : 8 + (i + 1) * 40] for i in range(returned)]
        return TraceWindow(available=available, entries=entries)

    def bp_set(
        self,
        *,
        kind: int,
        address: int,
        length: int = 1,
        flags: int = BP_FLAG_ENABLED,
        access: int = BP_ACCESS_NONE,
        domain: int = 0,
        addr_mask: int = 0xFFFFFFFF,
        data_value: int = 0,
        data_mask: int = 0xFF,
        ignore_count: int = 0,
    ) -> int:
        if not self._handshaked:
            raise RuntimeError("hello() required before bp_set()")
        payload = struct.pack(
            "<BBBBIIIIIII",
            kind & 0xFF,
            flags & 0xFF,
            access & 0xFF,
            0,
            domain,
            address,
            length,
            addr_mask,
            data_value,
            data_mask,
            ignore_count,
        )
        reply = self.request(BP_SET, payload)
        if len(reply) != 4:
            raise ProtocolError(0, f"BP_SET reply length {len(reply)}, expected 4")
        (bp_id,) = struct.unpack("<I", reply)
        return bp_id

    def bp_clear(self, bp_id: int) -> None:
        if not self._handshaked:
            raise RuntimeError("hello() required before bp_clear()")
        reply = self.request(BP_CLEAR, struct.pack("<I", bp_id))
        if reply:
            raise ProtocolError(0, f"BP_CLEAR reply not empty ({len(reply)} bytes)")

    def bp_clear_all(self) -> None:
        if not self._handshaked:
            raise RuntimeError("hello() required before bp_clear_all()")
        reply = self.request(BP_CLEAR_ALL, b"")
        if reply:
            raise ProtocolError(0, f"BP_CLEAR_ALL reply not empty ({len(reply)} bytes)")

    def bp_enable(self, bp_id: int, enabled: bool = True) -> None:
        if not self._handshaked:
            raise RuntimeError("hello() required before bp_enable()")
        reply = self.request(BP_ENABLE, struct.pack("<II", bp_id, 1 if enabled else 0))
        if reply:
            raise ProtocolError(0, f"BP_ENABLE reply not empty ({len(reply)} bytes)")

    def bp_list(self) -> list[BpInfo]:
        if not self._handshaked:
            raise RuntimeError("hello() required before bp_list()")
        reply = self.request(BP_LIST, b"")
        if len(reply) < 4:
            raise ProtocolError(0, f"BP_LIST reply too short ({len(reply)})")
        (count,) = struct.unpack_from("<I", reply, 0)
        need = 4 + 40 * count
        if len(reply) != need:
            raise ProtocolError(0, f"BP_LIST reply length {len(reply)}, expected {need}")
        out: list[BpInfo] = []
        for i in range(count):
            off = 4 + 40 * i
            bp_id, hit_count = struct.unpack_from("<II", reply, off)
            kind, flags, access, _pad = struct.unpack_from("<BBBB", reply, off + 8)
            domain, address, length, addr_mask, data_value, data_mask, ignore_count = (
                struct.unpack_from("<IIIIIII", reply, off + 12)
            )
            out.append(
                BpInfo(
                    id=bp_id,
                    hit_count=hit_count,
                    kind=kind,
                    flags=flags,
                    access=access,
                    domain=domain,
                    address=address,
                    length=length,
                    addr_mask=addr_mask,
                    data_value=data_value,
                    data_mask=data_mask,
                    ignore_count=ignore_count,
                )
            )
        return out

    def wait_event(self, *, timeout: float | None = 5.0) -> tuple[int, int, bytes]:
        """Block until an EVENT frame arrives. Returns (event_id, seq, data)."""
        if self._busy:
            raise RuntimeError("cannot wait_event while a request is outstanding")
        deadline = None if timeout is None else time.monotonic() + timeout
        while True:
            remaining = None
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("wait_event timed out")
            frame = self._recv_frame(timeout=remaining)
            if frame.type != EVENT:
                raise ProtocolError(
                    0, f"unexpected non-EVENT frame type 0x{frame.type:08x} while waiting"
                )
            if len(frame.payload) < 4:
                raise ProtocolError(0, "EVENT payload too short")
            (event_id,) = struct.unpack_from("<I", frame.payload, 0)
            data = frame.payload[4:]
            self._dispatch_event(frame)
            return event_id, frame.seq, data

    def wait_stopped(self, *, timeout: float | None = 5.0) -> StoppedEvent:
        """Wait for EVT_STOPPED and parse the payload."""
        while True:
            event_id, _seq, data = self.wait_event(timeout=timeout)
            if event_id != EVT_STOPPED:
                continue
            if len(data) < 32:
                raise ProtocolError(0, f"EVT_STOPPED data too short ({len(data)})")
            reason, bp_id, pc, eaddr, value = struct.unpack_from("<IIIII", data, 0)
            access, kind = data[20], data[21]
            execution_mode, trace_size = struct.unpack_from("<II", data, 24)
            trace = data[32 : 32 + trace_size] if trace_size else b""
            return StoppedEvent(
                reason=reason,
                bp_id=bp_id,
                pc=pc,
                eaddr=eaddr,
                value=value,
                access=access,
                kind=kind,
                execution_mode=execution_mode,
                trace=trace,
            )

    def read_mem(self, domain: int, address: int, length: int) -> bytes:
        """Send READMEM; return `length` bytes from domain/address.
        Domains: MAIN, MEGAII (IIgs), MAIN_RAW, MEGAII_RAW (IIgs)."""
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
        """Send WRITEMEM; poke `data` at domain/address.
        Domains: MAIN, MEGAII (IIgs), MAIN_RAW, MEGAII_RAW (IIgs)."""
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

    def tap_key(self, scancode: int, mod: int = 0, *, hold_s: float = 0.02) -> None:
        """Send keydown then keyup with the same scancode/mod."""
        self.key_down(scancode, mod)
        if hold_s > 0:
            time.sleep(hold_s)
        self.key_up(scancode, mod)

    def type_text(self, text: str, *, delay_s: float = 0.05, hold_s: float = 0.02) -> None:
        """Type printable ASCII (US layout). Newline becomes Return; shift held per glyph.

        delay_s: pause after each character (extra 2x after Return).
        hold_s: pause between keydown and keyup so the guest can see the strobe.
        """
        if not self._handshaked:
            raise RuntimeError("hello() required before type_text()")
        for ch in text:
            scancode, needs_shift = ascii_to_key(ch)
            if needs_shift:
                self.key_down(SCANCODE_LSHIFT, KMOD_LSHIFT)
                self.tap_key(scancode, KMOD_LSHIFT, hold_s=hold_s)
                self.key_up(SCANCODE_LSHIFT, 0)
            else:
                self.tap_key(scancode, 0, hold_s=hold_s)
            pause = delay_s
            if ch in "\n\r" and delay_s > 0:
                pause = delay_s * 3
            if pause > 0:
                time.sleep(pause)

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
