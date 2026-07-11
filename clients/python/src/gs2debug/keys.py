"""SDL3 scancode / keymod constants used by KEYEVENT helpers (US layout)."""

from __future__ import annotations

# SDL_Scancode values from SDL3/SDL_scancode.h
SCANCODE_A = 4
SCANCODE_B = 5
SCANCODE_C = 6
SCANCODE_D = 7
SCANCODE_E = 8
SCANCODE_F = 9
SCANCODE_G = 10
SCANCODE_H = 11
SCANCODE_I = 12
SCANCODE_J = 13
SCANCODE_K = 14
SCANCODE_L = 15
SCANCODE_M = 16
SCANCODE_N = 17
SCANCODE_O = 18
SCANCODE_P = 19
SCANCODE_Q = 20
SCANCODE_R = 21
SCANCODE_S = 22
SCANCODE_T = 23
SCANCODE_U = 24
SCANCODE_V = 25
SCANCODE_W = 26
SCANCODE_X = 27
SCANCODE_Y = 28
SCANCODE_Z = 29

SCANCODE_1 = 30
SCANCODE_2 = 31
SCANCODE_3 = 32
SCANCODE_4 = 33
SCANCODE_5 = 34
SCANCODE_6 = 35
SCANCODE_7 = 36
SCANCODE_8 = 37
SCANCODE_9 = 38
SCANCODE_0 = 39

SCANCODE_RETURN = 40
SCANCODE_ESCAPE = 41
SCANCODE_BACKSPACE = 42
SCANCODE_TAB = 43
SCANCODE_SPACE = 44

SCANCODE_MINUS = 45
SCANCODE_EQUALS = 46
SCANCODE_LEFTBRACKET = 47
SCANCODE_RIGHTBRACKET = 48
SCANCODE_BACKSLASH = 49
SCANCODE_SEMICOLON = 51
SCANCODE_APOSTROPHE = 52
SCANCODE_GRAVE = 53
SCANCODE_COMMA = 54
SCANCODE_PERIOD = 55
SCANCODE_SLASH = 56

SCANCODE_F12 = 69

SCANCODE_LCTRL = 224
SCANCODE_LSHIFT = 225
SCANCODE_RSHIFT = 229
SCANCODE_RCTRL = 228

# SDL_Keymod (SDL3/SDL_keycode.h)
KMOD_NONE = 0
KMOD_LSHIFT = 0x0001
KMOD_RSHIFT = 0x0002
KMOD_LCTRL = 0x0040
KMOD_RCTRL = 0x0080
KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL
KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT

# US QWERTY: printable ASCII -> (scancode, needs_shift)
_ASCII_KEYS: dict[str, tuple[int, bool]] = {
    " ": (SCANCODE_SPACE, False),
    "\n": (SCANCODE_RETURN, False),
    "\r": (SCANCODE_RETURN, False),
    "\t": (SCANCODE_TAB, False),
    "`": (SCANCODE_GRAVE, False),
    "~": (SCANCODE_GRAVE, True),
    "1": (SCANCODE_1, False),
    "!": (SCANCODE_1, True),
    "2": (SCANCODE_2, False),
    "@": (SCANCODE_2, True),
    "3": (SCANCODE_3, False),
    "#": (SCANCODE_3, True),
    "4": (SCANCODE_4, False),
    "$": (SCANCODE_4, True),
    "5": (SCANCODE_5, False),
    "%": (SCANCODE_5, True),
    "6": (SCANCODE_6, False),
    "^": (SCANCODE_6, True),
    "7": (SCANCODE_7, False),
    "&": (SCANCODE_7, True),
    "8": (SCANCODE_8, False),
    "*": (SCANCODE_8, True),
    "9": (SCANCODE_9, False),
    "(": (SCANCODE_9, True),
    "0": (SCANCODE_0, False),
    ")": (SCANCODE_0, True),
    "-": (SCANCODE_MINUS, False),
    "_": (SCANCODE_MINUS, True),
    "=": (SCANCODE_EQUALS, False),
    "+": (SCANCODE_EQUALS, True),
    "[": (SCANCODE_LEFTBRACKET, False),
    "{": (SCANCODE_LEFTBRACKET, True),
    "]": (SCANCODE_RIGHTBRACKET, False),
    "}": (SCANCODE_RIGHTBRACKET, True),
    "\\": (SCANCODE_BACKSLASH, False),
    "|": (SCANCODE_BACKSLASH, True),
    ";": (SCANCODE_SEMICOLON, False),
    ":": (SCANCODE_SEMICOLON, True),
    "'": (SCANCODE_APOSTROPHE, False),
    '"': (SCANCODE_APOSTROPHE, True),
    ",": (SCANCODE_COMMA, False),
    "<": (SCANCODE_COMMA, True),
    ".": (SCANCODE_PERIOD, False),
    ">": (SCANCODE_PERIOD, True),
    "/": (SCANCODE_SLASH, False),
    "?": (SCANCODE_SLASH, True),
}

for _i, _ch in enumerate("abcdefghijklmnopqrstuvwxyz"):
    _sc = SCANCODE_A + _i
    _ASCII_KEYS[_ch] = (_sc, False)
    _ASCII_KEYS[_ch.upper()] = (_sc, True)


def ascii_to_key(ch: str) -> tuple[int, bool]:
    """Return (scancode, needs_shift) for a single ASCII character."""
    if len(ch) != 1:
        raise ValueError("ascii_to_key expects a single character")
    try:
        return _ASCII_KEYS[ch]
    except KeyError as exc:
        raise ValueError(f"unsupported character for type_text: {ch!r}") from exc
