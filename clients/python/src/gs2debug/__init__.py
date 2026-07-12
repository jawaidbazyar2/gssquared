"""GSSquared external debug protocol client."""

from .client import Client, HelloInfo, StatusInfo
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
    MEM_MAIN_RAW,
    MEM_MEGAII,
    MEM_MEGAII_RAW,
    PING,
    READMEM,
    RESET,
    WRITEMEM,
)

# PlatformId_t / CLI -p N
PLATFORM_APPLE_II = 0
PLATFORM_APPLE_II_PLUS = 1
PLATFORM_APPLE_IIE = 2
PLATFORM_APPLE_IIE_ENHANCED = 3
PLATFORM_APPLE_IIE_65816 = 4
PLATFORM_APPLE_IIGS = 5

__all__ = [
    "Client",
    "HelloInfo",
    "StatusInfo",
    "ProtocolError",
    "HELLO",
    "PING",
    "ERROR",
    "EVENT",
    "GET_STATUS",
    "RESET",
    "READMEM",
    "WRITEMEM",
    "KEYEVENT",
    "MEM_MAIN",
    "MEM_MEGAII",
    "MEM_ENSONIQ",
    "MEM_ADBMICRO",
    "MEM_MAIN_RAW",
    "MEM_MEGAII_RAW",
    "PLATFORM_APPLE_II",
    "PLATFORM_APPLE_II_PLUS",
    "PLATFORM_APPLE_IIE",
    "PLATFORM_APPLE_IIE_ENHANCED",
    "PLATFORM_APPLE_IIE_65816",
    "PLATFORM_APPLE_IIGS",
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
