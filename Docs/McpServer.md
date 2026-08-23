# GS2 Debug Protocol — MCP server (decision)

Decision record from 2026-08-20. Not an implementation spec.

Expose the [external debug protocol](DebugProtocol.md) to LLM hosts (Cursor, Claude Desktop, …) as an MCP server. The emulator stays JSON-free; MCP lives entirely on the host side.

## Decision

**Go, separate repo, prebuilt binaries.** Developer-only extra — not shipped inside the GSSquared app bundle / `.exe` / AppImage.

| Item | Choice |
|------|--------|
| Language | Go |
| Packaging | Separate repository; GitHub Releases per OS/arch |
| Install | Download a binary, point `mcp.json` at it (“download and go”) |
| Runtime | None on the user’s machine (no Python, no Node, no Go toolchain) |
| Tool surface | 1:1 with protocol / `Client` methods — no workflows in the server |
| Contract | [DebugProtocol.md](DebugProtocol.md) (copy or submodule into the MCP repo, plus golden frame fixtures) |

Python `gs2debug` ([DebugClient.md](DebugClient.md), [gs2debug.md](gs2debug.md)) stays the in-tree client for scripts, examples, and smoke tests. The Go binary is a second client that happens to speak MCP.

## Why Go

MCP itself is language-agnostic: the host spawns a subprocess and speaks JSON-RPC over stdio. Language choice is install story, who already speaks the wire protocol, and process overhead.

- **Download and go.** Static binaries for darwin-arm64/amd64, linux-amd64/arm64, windows-amd64. Users never install a toolchain.
- **Low-overhead sidecar.** Typical stdio MCP process: a handful of goroutines (stdin, maybe a debug-socket reader for `EVT_*`), ~10–20 MB RSS, near-zero CPU while the agent is thinking. Startup is tens of milliseconds. This is not on the emulation hot path — the emu still owns pause/step/READMEM latency — but the sidecar should not tax the machine while debugging.
- **Small surface.** Frame pack/unpack, a `Client` mirroring `gs2debug`, MCP tool wrappers, stdio. Boring socket + JSON-RPC glue.

Python was the obvious *in-tree* wrap (`FastMCP` on `gs2debug`) and remains the right library for agents that already run Python. It was rejected for the shipped MCP server because that product is a separately distributed developer tool, and Python is the runtime we do not want users (or us) to deal with for this.

TypeScript/`npx` is the common MCP copy-paste default, but would duplicate the wire client (or shell out to Python). A TS client is still fine later if some other host wants TypeScript natively; it is not a reason to start the MCP server there.

C++/in-process MCP stays a non-goal ([DebugProtocol.md](DebugProtocol.md) v1): no JSON, MCP, or DSL in the emulator.

## Architecture

```
LLM host  --stdio JSON-RPC-->  gs2-mcp (Go)  --binary frames-->  GSSquared --debug socket
```

- Host: Cursor / Claude Desktop / etc. (`mcp.json` command + args + env for the socket path).
- `gs2-mcp`: thin adapter. Tool `read_mem` → `Client.ReadMem()`, tool `pause` → `Client.Pause()`, same for HELLO, run-control, BPs, keys, mount, …
- GS2 debug thread: existing framed protocol. Unchanged.

Workflows (reset → type → peek → wait) stay in the agent, not in the server. That matches the protocol rule: host-driven imperative commands, no DSL in GS2.

## What still blocks “any platform”

Not MCP language.

1. **Transport to the emu.** AF_UNIX first (macOS/Linux; Windows AF_UNIX on Win10+). Named pipes / TCP are still “later” in the protocol doc. The MCP server inherits that. TCP (or `--debug-tcp`) is what makes Windows boring.
2. **Who launches the server.** MCP hosts spawn a command. A path to a released binary is the whole install.

## Non-goals (this decision)

- Shipping MCP inside the GSSquared application for end users.
- Replacing `clients/python` / `gs2debug`.
- Putting workflows, retries, or policy in the MCP server.
- Embedding MCP or JSON in the emulator.
