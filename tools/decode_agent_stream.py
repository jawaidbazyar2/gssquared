#!/usr/bin/env python3
"""
Decode the GSSquared agent → compositor wire protocol into human-readable
lines.

Two input modes are auto-detected:

  - xxd / hexdump style ASCII (lines like
        "00000000: 0006 0001 0000 0003 ..."
    which is what `socat ... | xxd` produces — the format used in the
    captures pasted into chat).

  - raw binary on stdin (use this in a pipe straight from socat:
        socat -u UNIX-CONNECT:/tmp/iigs-agent.sock - | tools/decode_agent_stream.py

Tool-call selectors are decoded against tools/iigs_tools.tsv, which is
generated from Apple/Roger Wagner Publishing's Merlin assembler macros
(_NewWindow MAC / Tool $90E etc.). 1262 tool-set/function name pairs.

Usage:
    decode_agent_stream.py                 # read raw binary from stdin
    decode_agent_stream.py path/to/cap.txt # parse an xxd-style capture file
    decode_agent_stream.py - < cap.txt     # ditto, from stdin
"""

import os
import re
import sys
from typing import Dict, Tuple

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
TOOL_TSV = os.path.join(THIS_DIR, "iigs_tools.tsv")

TS_NAMES = {
    0x01: "ToolLocator", 0x02: "MemoryMgr",  0x03: "MiscTools",
    0x04: "QuickDrawII", 0x05: "DeskMgr",    0x06: "EventMgr",
    0x07: "Scheduler",   0x08: "SoundMgr",   0x09: "ADBTool",
    0x0A: "SANE",        0x0B: "IntMath",    0x0C: "TextTools",
    0x0E: "WindowMgr",   0x0F: "MenuMgr",    0x10: "ControlMgr",
    0x11: "QDIIAux",     0x12: "PrintMgr",   0x13: "LineEdit",
    0x14: "DialogMgr",   0x15: "ScrapMgr",   0x16: "StdFile",
    0x17: "NoteSynth",   0x18: "NoteSeq",    0x19: "FontMgr",
    0x1A: "ListMgr",     0x1B: "ACE",        0x1C: "ResourceMgr",
    0x1E: "TextEdit",    0x20: "MIDISynth",  0x21: "MIDIDrv",
    0x22: "Tool222",     0x23: "Media",      0x32: "GTE",
}


def load_tool_table(path: str) -> Dict[int, str]:
    """Parse iigs_tools.tsv into {x_register: name}."""
    table: Dict[int, str] = {}
    if not os.path.exists(path):
        return table
    with open(path) as fp:
        for line in fp:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            try:
                x = int(parts[0], 16)
            except ValueError:
                continue
            table[x] = parts[1]
    return table


def is_xxd_line(line: str) -> bool:
    return bool(re.match(r"^\s*[0-9a-fA-F]{4,}:\s+[0-9a-fA-F]", line))


def slurp_bytes(input_path: str) -> bytes:
    """
    Read either an xxd-style file or raw binary.
    input_path == "-" means stdin.
    """
    if input_path == "-":
        data = sys.stdin.buffer.read()
        # Heuristic: if it looks like text + xxd, parse as xxd. Otherwise
        # treat as raw binary.
        try:
            text = data.decode("ascii")
        except UnicodeDecodeError:
            return data
        if any(is_xxd_line(line) for line in text.splitlines()[:8]):
            return parse_xxd(text)
        return data
    with open(input_path, "rb") as fp:
        raw = fp.read()
    try:
        text = raw.decode("ascii")
    except UnicodeDecodeError:
        return raw
    if any(is_xxd_line(line) for line in text.splitlines()[:8]):
        return parse_xxd(text)
    return raw


def parse_xxd(text: str) -> bytes:
    """Pull bytes out of `xxd` / `hexdump -C` style output."""
    out = bytearray()
    for line in text.splitlines():
        m = re.match(r"^\s*[0-9a-fA-F]+:\s+([0-9a-fA-F ]+?)(?:\s{2,}|$)", line)
        if not m:
            continue
        hexpart = m.group(1).replace(" ", "")
        # Even-length only.
        if len(hexpart) % 2:
            hexpart = hexpart[:-1]
        try:
            out.extend(bytes.fromhex(hexpart))
        except ValueError:
            continue
    return bytes(out)


# Wire protocol tags (kept in sync with src/agent/Protocol.hpp).
TAG_HELLO       = 0x00
TAG_VBL         = 0x01
TAG_TOOL_CALL   = 0x02
TAG_MEM_WRITE   = 0x03
TAG_TOOL_RETURN = 0x04
TAG_MEM_BLOB    = 0x05


def _mem_region(addr: int) -> str:
    """Best-effort name of the region a 24-bit video memory address falls in."""
    bank = (addr >> 16) & 0xFF
    off = addr & 0xFFFF
    if bank == 0xE0:
        if 0xC000 <= off < 0xC100: return "softswitch"
        if 0x0400 <= off < 0x0800: return "text-page1"
        if 0x0800 <= off < 0x0C00: return "text-page2"
        if 0x2000 <= off < 0x4000: return "hires-page1"
        if 0x4000 <= off < 0x6000: return "hires-page2"
        return "$E0"
    if bank == 0xE1:
        if 0x2000 <= off < 0xA000: return "shr"
        return "$E1"
    return f"${bank:02x}"


def decode(buf: bytes, tools: Dict[int, str]) -> None:
    i = 0
    n = len(buf)
    pkt_count = 0
    while i + 2 <= n:
        L = (buf[i] << 8) | buf[i + 1]
        if L == 0 or i + 2 + L > n:
            sys.stderr.write(
                f"# truncated/zero packet at offset {i}, stopping\n"
            )
            break
        tag = buf[i + 2]
        payload = buf[i + 3 : i + 2 + L]
        line = _format(tag, payload, tools)
        if line is not None:
            print(line)
        pkt_count += 1
        i += 2 + L
    sys.stderr.write(f"# {pkt_count} packets, {i}/{n} bytes consumed\n")


def _format(tag: int, payload: bytes, tools: Dict[int, str]) -> str:
    if tag == TAG_HELLO and len(payload) >= 5:
        version = payload[0]
        caps = int.from_bytes(payload[1:5], "big")
        return f"HELLO              version={version} caps=0x{caps:08x}"
    if tag == TAG_VBL and len(payload) >= 4:
        seq = int.from_bytes(payload[0:4], "big")
        return f"VBL                seq={seq}"
    if tag == TAG_MEM_WRITE and len(payload) >= 5:
        addr = int.from_bytes(payload[0:4], "big") & 0x00FFFFFF
        data = payload[4]
        return (
            f"MEM_WRITE          {_mem_region(addr):<11s}"
            f"  addr=${addr:06x} data=${data:02x}"
        )
    if tag == TAG_MEM_BLOB and len(payload) >= 6:
        addr = int.from_bytes(payload[0:4], "big") & 0x00FFFFFF
        blen = int.from_bytes(payload[4:6], "big")
        return f"MEM_BLOB           addr=${addr:06x} len={blen}"
    if tag in (TAG_TOOL_CALL, TAG_TOOL_RETURN) and len(payload) >= 12:
        x = int.from_bytes(payload[0:2], "big")
        a = int.from_bytes(payload[2:4], "big")
        y = int.from_bytes(payload[4:6], "big")
        sp = int.from_bytes(payload[6:8], "big")
        slen = payload[8] if len(payload) > 8 else 0
        stack = bytes(payload[9 : 9 + slen]) if slen else b""
        ts = x & 0xFF
        fn = (x >> 8) & 0xFF
        ts_name = TS_NAMES.get(ts, f"ts{ts:02x}")
        fallback_fn = tools.get(x, f"fn{fn:02x}")
        label = "TOOL_CALL" if tag == TAG_TOOL_CALL else "TOOL_RETURN"

        # Try signature-based decode first; fall back to registers-only.
        try:
            from iigs_tool_args import format_call as _fmt_call
            from iigs_tool_args import format_return as _fmt_ret
            if tag == TAG_TOOL_CALL:
                argstr = _fmt_call(x, stack, fallback_fn)
            else:
                argstr = _fmt_ret(x, stack, fallback_fn)
            if argstr != fallback_fn:
                return f"{label:<11s}        {ts_name}/{argstr}"
        except ImportError:
            pass

        return (
            f"{label:<11s}        {ts_name}/{fallback_fn}"
            f"  x=0x{x:04x} a=0x{a:04x} y=0x{y:04x} sp=0x{sp:04x}"
        )
    return f"UNKNOWN tag=0x{tag:02x} len={len(payload)+1} payload={payload.hex()}"


def main() -> int:
    if len(sys.argv) > 1:
        input_path = sys.argv[1]
    else:
        input_path = "-"
    tools = load_tool_table(TOOL_TSV)
    if not tools:
        sys.stderr.write(
            f"# warning: tool table not found at {TOOL_TSV}, "
            f"calls will show numeric selectors only\n"
        )
    buf = slurp_bytes(input_path)
    sys.stderr.write(f"# {len(buf)} bytes loaded, {len(tools)} tool names\n")
    decode(buf, tools)
    return 0


if __name__ == "__main__":
    sys.exit(main())
