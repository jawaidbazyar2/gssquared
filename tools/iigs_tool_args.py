"""
Per-tool argument signatures for IIgs Toolbox calls captured by the
agent. Used by tools/decode_agent_stream.py to decode the 16 stack
bytes accompanying each TOOL_CALL packet into named args.

Conventions
-----------
The IIgs Toolbox uses Pascal-style calling: the caller pushes a return
slot (if the function returns a value), then pushes args in declaration
order, then `JSL $E10000`. After the JSL pushes its 3-byte return PC,
the agent reads 16 bytes starting at `SP+4` — the first byte of the
caller's pushes after the return PC.

Because args are pushed left-to-right and the stack grows downward, the
LAST argument in the function signature ends up at the LOWEST offset in
the captured bytes. So signatures here list args in **stack order**,
which is reverse of declaration order. We annotate the source-order
declaration in the comment for each tool.

Type tags:
    "ptr32"   4-byte little-endian long pointer (24-bit address + 1 spare)
    "word"    2-byte little-endian signed integer
    "long"    4-byte little-endian signed integer
    "ret2"    2-byte return slot (caller-allocated; not really an "arg")
    "ret4"    4-byte return slot
"""

from typing import Dict, List, Tuple

# (tool_name, [(field_name, type_tag), ...])
TOOL_SIGS: Dict[int, Tuple[str, List[Tuple[str, str]]]] = {
    # Window Manager (tool set $0E). Most ops take a single WindowPtr.
    # Order in the signature is stack order = reverse of declaration order.

    # NewWindow(paramList: Ptr): WindowPtr;
    0x090E: ("NewWindow", [("paramListPtr", "ptr32"), ("retWindow", "ret4")]),

    # CloseWindow(theWindow: WindowPtr);
    0x0B0E: ("CloseWindow", [("theWindow", "ptr32")]),

    # SetWTitle(theWindow: WindowPtr; title: Ptr);
    # Macro: PxL ]1;]2 — title pushed last lands at offset 0, theWindow
    # at offset 4. (Different from PHWL, which reverses the order.)
    0x0D0E: ("SetWTitle",
             [("titlePtr", "ptr32"), ("theWindow", "ptr32")]),

    # SelectWindow(theWindow: WindowPtr);
    0x110E: ("SelectWindow", [("theWindow", "ptr32")]),

    # HideWindow(theWindow: WindowPtr);
    0x120E: ("HideWindow", [("theWindow", "ptr32")]),

    # ShowWindow(theWindow: WindowPtr);
    0x130E: ("ShowWindow", [("theWindow", "ptr32")]),

    # SendBehind(theWindow: WindowPtr; behindWindow: WindowPtr);
    0x140E: ("SendBehind",
             [("behindWindow", "ptr32"), ("theWindow", "ptr32")]),

    # FrontWindow: WindowPtr;
    0x150E: ("FrontWindow", [("retWindow", "ret4")]),

    # FindWindow(thePoint: Point; var whichWindow: WindowPtr): Integer;
    # — too situational; punt for now.

    # TrackGoAway(theWindow: WindowPtr; thePoint: Point): Boolean;
    # Empirically: theWindow lands at stack offset 0, thePoint at offset 4
    # — `PHWL` in the supermacs macro pushes in opposite order from `PHL`.
    0x180E: ("TrackGoAway",
             [("theWindow", "ptr32"), ("thePoint", "long"),
              ("retBool", "ret2")]),

    # MoveWindow(h, v: Integer; theWindow: WindowPtr);
    0x190E: ("MoveWindow",
             [("theWindow", "ptr32"), ("v", "word"), ("h", "word")]),

    # DragWindow(theWindow: WindowPtr; thePoint: Point; bounds: Ptr);
    # Empirical: theWindow lands at stack offset 0 (same `PxL`-reverses
    # pattern as TrackGoAway). thePoint frequently reads as zero in
    # captures — the macro arity (PxW + PxL) doesn't match a clean 3-arg
    # Pascal signature, so the middle field is best-effort and the
    # bounds field is mostly a sanity check.
    0x1A0E: ("DragWindow",
             [("theWindow", "ptr32"), ("thePoint", "long"),
              ("bounds", "ptr32")]),

    # GrowWindow(theWindow: WindowPtr; bounds: Ptr): LongInt;
    0x1B0E: ("GrowWindow",
             [("bounds", "ptr32"), ("theWindow", "ptr32"),
              ("retSize", "ret4")]),

    # SizeWindow(w, h: Integer; theWindow: WindowPtr);
    0x1C0E: ("SizeWindow",
             [("theWindow", "ptr32"), ("h", "word"), ("w", "word")]),

    # BringToFront(theWindow: WindowPtr);
    0x230E: ("BringToFront", [("theWindow", "ptr32")]),

    # SendToBack(theWindow: WindowPtr);
    0x240E: ("SendToBack", [("theWindow", "ptr32")]),

    # TrackZoom(theWindow: WindowPtr; thePoint: Point): Boolean;
    # Macro is the same shape as TrackGoAway (P1SW + PHWL ]2;]3), so two
    # longs only — no bounds arg, despite some references showing one.
    # theWindow lands at stack offset 0.
    0x260E: ("TrackZoom",
             [("theWindow", "ptr32"), ("thePoint", "long"),
              ("retBool", "ret2")]),

    # ZoomWindow(theWindow: WindowPtr);
    0x270E: ("ZoomWindow", [("theWindow", "ptr32")]),

    # GetNextWindow(theWindow: WindowPtr): WindowPtr;
    0x2A0E: ("GetNextWindow", [("theWindow", "ptr32"), ("retNext", "ret4")]),

    # Menu Manager — startPt and char/event args.

    # MenuSelect(startPt: Point): LongInt;
    0x2B0F: ("MenuSelect", [("startPt", "long"), ("retSel", "ret4")]),

    # MenuKey(theEvent: Ptr): LongInt;
    0x2C0F: ("MenuKey", [("theEvent", "ptr32"), ("retSel", "ret4")]),

    # NewMenu(menuStr: Ptr): MenuHandle;
    0x2D0F: ("NewMenu", [("menuStr", "ptr32"), ("retMenuH", "ret4")]),

    # InsertMenu(theMenu: MenuHandle; beforeID: Integer);
    0x0D0F: ("InsertMenu",
             [("beforeID", "word"), ("theMenu", "ptr32")]),
}


def decode_args(tool_x: int, stack: bytes) -> Tuple[str, List[Tuple[str, str, int]]]:
    """Look up the tool's signature and decode the 16-byte stack
    capture into (field_name, type_tag, value) triples.

    Returns (tool_name, list_of_decoded_fields). If we don't have a
    signature for this tool, returns (None, []) — caller falls back to
    the raw register-only display.
    """
    sig = TOOL_SIGS.get(tool_x)
    if sig is None:
        return ("", [])

    name, fields = sig
    decoded: List[Tuple[str, str, int]] = []
    off = 0
    for field_name, type_tag in fields:
        size = {"word": 2, "long": 4, "ptr32": 4, "ret2": 2, "ret4": 4}.get(type_tag, 0)
        if off + size > len(stack):
            break
        chunk = stack[off : off + size]
        # Little-endian — IIgs / 65816 native byte order.
        value = int.from_bytes(chunk, "little")
        decoded.append((field_name, type_tag, value))
        off += size
    return (name, decoded)


def format_field(field_name: str, type_tag: str, value: int) -> str:
    if type_tag == "ptr32":
        # Long pointer — show low 24 bits as the address.
        return f"{field_name}=${value & 0x00FFFFFF:06x}"
    if type_tag == "long":
        return f"{field_name}=${value & 0xFFFFFFFF:08x}"
    if type_tag == "word":
        # Treat as signed for display when small; otherwise hex.
        sval = value if value < 0x8000 else value - 0x10000
        return f"{field_name}={sval}"
    if type_tag in ("ret2", "ret4"):
        return f"[{field_name}=${value:0{2 * 4 if type_tag == 'ret4' else 2 * 2}x}]"
    return f"{field_name}=?"


def format_return(tool_x: int, stack: bytes,
                  fallback_name: str = "") -> str:
    """Render a TOOL_RETURN. Same signature lookup as format_call but
    surfaces the return slot(s) only — the call-side args at this point
    are unreliable (some tools leave the area clean, others use it as
    scratch during their work). The return slot itself is what the
    dispatcher just wrote, so it's authoritative."""
    name, fields = decode_args(tool_x, stack)
    if not name:
        return fallback_name
    rets = [format_field(*f) for f in fields if f[1] in ("ret2", "ret4")]
    if rets:
        ret_str = ", ".join(s.strip("[]") for s in rets)
        return f"{name} → {ret_str}"
    return f"{name}() returned"


def format_call(tool_x: int, stack: bytes,
                fallback_name: str = "",
                show_ret_slots: bool = False) -> str:
    """Render a TOOL_CALL into a one-line description like
    'WindowMgr/NewWindow(paramListPtr=$0123ab)'. If we don't have a
    signature, returns just the tool name with no args.

    Return slots are hidden by default. They sit at the bottom of the
    stack capture but the agent fires at function *entry*, so they
    contain whatever the caller pushed (usually zero or stale stack);
    a return-value hook on dispatcher exit will give us the real values.
    """
    name, fields = decode_args(tool_x, stack)
    if not name:
        return fallback_name
    parts = [format_field(*f) for f in fields if f[1] not in ("ret2", "ret4")]
    arg_str = ", ".join(parts) if parts else ""
    if show_ret_slots:
        rets = [format_field(*f) for f in fields if f[1] in ("ret2", "ret4")]
        suffix = " " + " ".join(rets) if rets else ""
    else:
        suffix = ""
    return f"{name}({arg_str}){suffix}"
