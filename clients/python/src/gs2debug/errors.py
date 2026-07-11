"""Protocol error raised when the server returns an ERROR frame."""

from __future__ import annotations


class ProtocolError(Exception):
    def __init__(self, code: int, message: str = "") -> None:
        self.code = code
        self.message = message
        super().__init__(f"protocol error {code}: {message}" if message else f"protocol error {code}")
