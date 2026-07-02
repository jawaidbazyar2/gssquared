#!/usr/bin/env python3
"""
End-to-end regression test for the GSSquared MCP server.

Launches a headless emulator, drives it entirely over the MCP socket, and
asserts real behaviour (reset vector, memory r/w, mem_diff, disasm, and a
full Applesoft BASIC program run). Exits 0 if all checks pass, 1 otherwise.

Usage:  python3 tools/mcp_test.py [path-to-GSSquared]
(defaults to ./build/GSSquared relative to the repo root)
"""
import json, os, socket, subprocess, sys, time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "build", "GSSquared")
SOCK = "/tmp/gs2-mcp-test.sock"

results = []
def check(name, ok, detail=""):
    results.append(ok)
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f"  ({detail})" if detail and not ok else ""))

def main():
    if os.path.exists(SOCK):
        os.remove(SOCK)
    env = dict(os.environ, GS2_HEADLESS="1", GS2_MCP_SOCKET=SOCK, SDL_AUDIODRIVER="dummy")
    emu = subprocess.Popen([BIN, "-p", "1"], env=env,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        # connect (wait for the emulator to come up)
        s = None
        for _ in range(80):
            if emu.poll() is not None:
                print("emulator exited early"); return 1
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect(SOCK); break
            except OSError:
                time.sleep(0.25)
        if s is None:
            print("could not connect to MCP socket"); return 1

        f = s.makefile("rwb", buffering=0); _id = [0]
        def call(method, params=None):
            _id[0] += 1
            m = {"jsonrpc": "2.0", "id": _id[0], "method": method}
            if params is not None: m["params"] = params
            f.write((json.dumps(m) + "\n").encode())
            return json.loads(f.readline())
        def tool(n, a={}):
            r = call("tools/call", {"name": n, "arguments": a})
            return json.loads(r["result"]["content"][0]["text"])
        def screen():
            return tool("screen_text").get("rows", [])

        init = call("initialize", {"protocolVersion": "2024-11-05", "capabilities": {}})
        check("initialize", init.get("result", {}).get("serverInfo", {}).get("name") == "gssquared-mcp")

        names = {t["name"] for t in call("tools/list")["result"]["tools"]}
        expected = {"regs","peek","poke","reset","step","until_pc","disasm",
                    "screen_text","mem_diff","setreg","type","mount_disk","pause","resume"}
        check("tools/list has all tools", expected <= names, f"missing {expected - names}")

        rv = tool("peek", {"addr": 0xFFFC, "len": 2})["bytes"]
        check("reset vector $FFFC = $FA62", rv == [0x62, 0xFA], f"got {rv}")

        tool("poke", {"addr": 0x300, "bytes": [0x11, 0x22, 0x33]})
        rb = tool("peek", {"addr": 0x300, "len": 3})["bytes"]
        check("poke/peek roundtrip", rb == [0x11, 0x22, 0x33], f"got {rb}")

        tool("mem_diff", {"action": "snapshot", "addr": 0x300, "len": 4})
        tool("poke", {"addr": 0x301, "bytes": [0xFF]})
        d = tool("mem_diff", {"action": "diff"})
        ok = d.get("changed") == 1 and d["changes"][0]["addr"] == 0x301 and d["changes"][0]["now"] == 0xFF
        check("mem_diff detects the changed byte", ok, str(d))

        dis = tool("disasm", {"addr": 0xFA62, "count": 1})["lines"]
        check("disasm returns a line", len(dis) == 1 and dis[0].startswith("FA62"), str(dis))

        # Full BASIC program run.
        tool("reset", {"cold": True}); time.sleep(0.2)
        tool("setreg", {"reg": "pc", "value": 0xE000}); time.sleep(0.6)
        rows = screen()
        check("reset into Applesoft (] prompt)", any(r.startswith("]") for r in rows), str([r for r in rows if r.strip()]))

        tool("type", {"text": '10 PRINT "HELLO, WORLD"\n20 GOTO 10\n'})
        tool("type", {"text": 'RUN\n'}); time.sleep(0.4)
        tool("pause")
        rows = screen()
        check("BASIC program output present", any("HELLO, WORLD" in r for r in rows),
              str([r for r in rows if r.strip()][:3]))

        print(f"\n{sum(results)}/{len(results)} checks passed")
        return 0 if all(results) else 1
    finally:
        emu.terminate()
        try: emu.wait(timeout=3)
        except subprocess.TimeoutExpired: emu.kill()
        if os.path.exists(SOCK): os.remove(SOCK)

if __name__ == "__main__":
    sys.exit(main())
