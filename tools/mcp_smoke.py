#!/usr/bin/env python3
import socket, json, sys, time

SOCK = sys.argv[1] if len(sys.argv) > 1 else "/tmp/gs2-mcp.sock"

# wait for socket
for _ in range(80):
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(SOCK)
        break
    except OSError:
        time.sleep(0.25)
else:
    print("FAIL: could not connect to", SOCK); sys.exit(1)

f = s.makefile("rwb", buffering=0)

def call(method, params=None, _id=[0]):
    _id[0] += 1
    msg = {"jsonrpc": "2.0", "id": _id[0], "method": method}
    if params is not None:
        msg["params"] = params
    f.write((json.dumps(msg) + "\n").encode())
    line = f.readline()
    resp = json.loads(line)
    print(f"\n>>> {method} {params or ''}")
    print(json.dumps(resp, indent=2)[:900])
    return resp

call("initialize", {"protocolVersion": "2024-11-05", "capabilities": {}, "clientInfo": {"name": "smoke", "version": "0"}})
call("tools/list")
call("tools/call", {"name": "regs", "arguments": {}})
call("tools/call", {"name": "peek", "arguments": {"addr": 0xFFFC, "len": 4}})
call("tools/call", {"name": "disasm", "arguments": {"count": 4}})
call("tools/call", {"name": "step", "arguments": {"count": 5}})
call("tools/call", {"name": "regs", "arguments": {}})
call("tools/call", {"name": "poke", "arguments": {"addr": 0x0300, "bytes": [0xEA, 0xEA]}})
call("tools/call", {"name": "peek", "arguments": {"addr": 0x0300, "len": 2}})
call("tools/call", {"name": "until_pc", "arguments": {"addr": 0xFA62, "max": 200000}})
call("tools/call", {"name": "resume", "arguments": {}})
print("\nOK: smoke test completed")
