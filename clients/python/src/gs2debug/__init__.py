"""GSSquared external debug protocol client."""

from .client import Client, HelloInfo
from .errors import ProtocolError
from .keys import (
    KMOD_CTRL,
    KMOD_LCTRL,
    KMOD_LSHIFT,
    KMOD_NONE,
    KMOD_SHIFT,
    SCANCODE_F12,
    SCANCODE_LCTRL,
    SCANCODE_LSHIFT,
    SCANCODE_RETURN,
    SCANCODE_SPACE,
)
from .types import (
    ERROR,
    EVENT,
    GET_STATUS,
    HELLO,
    KEYEVENT,
    MEM_ADBMICRO,
    MEM_ENSONIQ,
    MEM_MAIN,
    MEM_MEGAII,
    PING,
    READMEM,
    WRITEMEM,
)

__all__ = [
    "Client",
    "HelloInfo",
    "ProtocolError",
    "HELLO",
    "PING",
    "ERROR",
    "EVENT",
    "GET_STATUS",
    "READMEM",
    "WRITEMEM",
    "KEYEVENT",
    "MEM_MAIN",
    "MEM_MEGAII",
    "MEM_ENSONIQ",
    "MEM_ADBMICRO",
    "KMOD_NONE",
    "KMOD_LSHIFT",
    "KMOD_SHIFT",
    "KMOD_LCTRL",
    "KMOD_CTRL",
    "SCANCODE_LCTRL",
    "SCANCODE_LSHIFT",
    "SCANCODE_F12",
    "SCANCODE_RETURN",
    "SCANCODE_SPACE",
]
