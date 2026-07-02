/*
 *   Copyright (c) 2026 GSSquared Agent contributors
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "agent/Agent.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

#include <SDL3/SDL.h>

#include "agent/Protocol.hpp"
#include "agent/UnixSocketTransport.hpp"
#include "cpu.hpp"
#include "mmus/mmu.hpp"

namespace agent {

Agent::Agent(Config cfg)
    : cfg_(std::move(cfg)),
      queue_(cfg_.queue_capacity),
      transport_(std::make_unique<UnixSocketTransport>(cfg_.socket_path)) {
    if (!transport_->start_listening()) {
        // Listen failed — agent is dead on arrival. computer_t still holds
        // a pointer to us; integration points check enabled() before calling
        // emit_*().
        std::fprintf(stderr, "[agent] disabled (listen failed)\n");
        return;
    }
    enabled_.store(true, std::memory_order_release);
    io_thread_ = std::thread([this] { io_thread_main(); });
    std::fprintf(stderr, "[agent] started (socket=%s, queue_capacity=%zu)\n",
                 cfg_.socket_path.c_str(), cfg_.queue_capacity);
}

Agent::~Agent() {
    // Order matters: wake the IO thread out of accept() first (transport
    // shutdown), then drain the queue so any pop_blocking returns. Then
    // join. The transport destructor will also call shutdown() but doing
    // it explicitly keeps the order obvious.
    if (transport_) transport_->shutdown();
    queue_.shutdown();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    std::fprintf(stderr, "[agent] stopped (dropped=%zu)\n", dropped());
}

void Agent::submit(std::vector<std::uint8_t> packet) {
    queue_.push(std::move(packet));
}

void Agent::emit_vbl() {
    submit(protocol::build_vbl(seq_.fetch_add(1, std::memory_order_relaxed)));
}

void Agent::emit_mem_write(std::uint32_t addr_24bit, std::uint8_t data) {
    submit(protocol::build_mem_write(addr_24bit, data));
}

void Agent::emit_mem_blob(std::uint32_t addr_24bit,
                          const std::uint8_t* data, std::size_t len) {
    submit(protocol::build_mem_blob(addr_24bit, data, len));
}

namespace {

// Read TOOL_CALL_STACK_BYTES of bank-0 stack starting at SP+offset.
// `sp` is the 16-bit stack pointer; the 65816 stack lives in bank 0
// in both emulation and native mode, so we read at addresses
// `sp + offset`, masked to 16 bits.
inline void read_stack(MMU* mmu, std::uint16_t sp, std::uint16_t offset,
                       std::uint8_t* out, std::size_t len) {
    if (mmu == nullptr) {
        for (std::size_t i = 0; i < len; ++i) out[i] = 0;
        return;
    }
    for (std::size_t i = 0; i < len; ++i) {
        const std::uint16_t addr =
            static_cast<std::uint16_t>((sp + offset + i) & 0xFFFFu);
        out[i] = mmu->read(addr);
    }
}

// Read N bytes from a 24-bit address into a small int. Little-endian
// is the IIgs convention for in-memory longs/words.
inline std::uint32_t read_le_long(MMU* mmu, std::uint32_t addr_24bit) {
    if (mmu == nullptr) return 0;
    return  static_cast<std::uint32_t>(mmu->read((addr_24bit + 0) & 0x00FFFFFFu))
         | (static_cast<std::uint32_t>(mmu->read((addr_24bit + 1) & 0x00FFFFFFu)) << 8)
         | (static_cast<std::uint32_t>(mmu->read((addr_24bit + 2) & 0x00FFFFFFu)) << 16)
         | (static_cast<std::uint32_t>(mmu->read((addr_24bit + 3) & 0x00FFFFFFu)) << 24);
}

inline std::int16_t read_le_signed_word(MMU* mmu, std::uint32_t addr_24bit) {
    if (mmu == nullptr) return 0;
    const std::uint16_t v =
        static_cast<std::uint16_t>(mmu->read((addr_24bit + 0) & 0x00FFFFFFu)) |
        (static_cast<std::uint16_t>(mmu->read((addr_24bit + 1) & 0x00FFFFFFu)) << 8);
    return static_cast<std::int16_t>(v);
}

// IIgs WindMgr offsets — derived from window.h's WindRec struct
// (ORCA/C library) and the windSize=$00D4 = 212-byte total. The
// public WindowPtr points at the GrafPort start; wNext lives 4 bytes
// before that (the WindMgr internally allocates an extra 4 bytes
// before each WindRec it returns to apps; comment in window.h:
// "wNext not included in record returned by ToolBox calls").
//
//   Field             offset from WindowPtr
//   wNext (long)            -4
//   GrafPort port          0..173      (174 bytes; windSize - 38)
//   ProcPtr wDefProc       174
//   LongWord wRefCon       178
//   ProcPtr wContDraw      182
//   LongWord wReserved     186
//   RegionHndl wStrucRgn   190     ← bounding rect lives behind this handle
//   RegionHndl wContRgn    194
//   RegionHndl wUpdateRgn  198
//   CtlRecHndl wControls   202
//   CtlRecHndl wFrameCtrls 206
//   Word wFrame            210
//                          212 = windSize
constexpr std::int32_t WINDREC_WNEXT_OFFSET   = -4;
constexpr std::uint32_t WINDREC_WSTRUCRGN_OFF = 190;

// IIgs Region (from QuickDraw II): u16 rgnSize then 8-byte rgnBBox
// (Rect: top, left, bottom, right; each s16 little-endian).
constexpr std::uint32_t REGION_BBOX_OFFSET = 2;

// Heuristic title finder: scan a window's memory region for a 4-byte
// aligned longword that derefs to a plausible Pascal-style title
// string (length-prefixed bytes 0..MAX_TITLE_LEN, mostly printable).
//
// Why heuristic instead of a fixed offset: GS/OS Finder doesn't call
// SetWTitle / NewWindow2 / GetWTitle in any path we can observe, so
// the only way to recover the title from a WindowPtr is to find where
// WindMgr stashed it. Empirically the title pointer lives somewhere in
// the WindRec or the wFrameCtrls control list it points to; rather
// than committing to a specific layout (which differs across ROM
// versions), we walk a 256-byte window and pick the first match.
constexpr std::uint8_t  MAX_TITLE_LEN     = 64;
constexpr std::uint32_t TITLE_SCAN_RANGE  = 256;

// Read a Pascal string at `ptr`. Returns length (0 if not a plausible
// title). Output goes into `out` (caller-provided buffer of at least
// MAX_TITLE_LEN bytes). "Plausible" = length 1..MAX_TITLE_LEN, all
// chars printable-ish (>= 0x20 and != 0xFF), and not all the same
// byte (rejects fill patterns like 0xC7C7C7…).
std::uint8_t try_read_pascal_title(MMU* mmu, std::uint32_t ptr,
                                   std::uint8_t* out) {
    if (mmu == nullptr || ptr == 0) return 0;
    ptr &= 0x00FFFFFFu;
    const std::uint8_t len = mmu->read(ptr);
    if (len == 0 || len > MAX_TITLE_LEN) return 0;
    bool printable = true;
    bool all_same = true;
    std::uint8_t first = 0;
    for (std::uint8_t i = 0; i < len; ++i) {
        const std::uint8_t c = mmu->read((ptr + 1u + i) & 0x00FFFFFFu);
        out[i] = c;
        if (c < 0x20 || c == 0x7F) printable = false;
        if (i == 0) first = c;
        else if (c != first) all_same = false;
    }
    if (!printable || all_same) return 0;
    return len;
}

// Scan WindowPtr..WindowPtr+TITLE_SCAN_RANGE for a longword that
// derefs to a plausible Pascal title. Returns title length (0 if
// not found); fills `out` with the chars (no length prefix).
std::uint8_t find_window_title(MMU* mmu, std::uint32_t window_ptr,
                               std::uint8_t* out) {
    if (mmu == nullptr) return 0;
    window_ptr &= 0x00FFFFFFu;
    for (std::uint32_t off = 0; off + 4 <= TITLE_SCAN_RANGE; off += 2) {
        const std::uint32_t cand =
            read_le_long(mmu, (window_ptr + off) & 0x00FFFFFFu);
        // Sanity-bound the candidate pointer: must be in a low RAM bank
        // (system heap), not a fill pattern.
        const std::uint32_t addr = cand & 0x00FFFFFFu;
        const std::uint8_t b2 = static_cast<std::uint8_t>(addr >> 16);
        if (b2 > 0x0F) continue;
        const std::uint8_t b1 = static_cast<std::uint8_t>(addr >> 8);
        const std::uint8_t b0 = static_cast<std::uint8_t>(addr);
        if (addr == 0) continue;
        if (b0 == b1 && b1 == b2) continue;
        // Try direct pointer first; fall back to handle (deref once
        // more) so we cover both "wTitle = char*" and "wTitle = char**"
        // storage conventions.
        const std::uint8_t direct = try_read_pascal_title(mmu, addr, out);
        if (direct > 0) return direct;
        const std::uint32_t inner =
            read_le_long(mmu, addr) & 0x00FFFFFFu;
        if (inner != 0 && inner != addr) {
            const std::uint8_t hop = try_read_pascal_title(mmu, inner, out);
            if (hop > 0) return hop;
        }
    }
    return 0;
}

// FrontWindow returns a long via the result-slot the caller pushed
// before the JSL. Per Pascal calling convention on IIgs Toolbox,
// that slot sits at sp_at_entry + 1 (just above SP and the empty
// dispatcher frame); same place the existing on_tool_return reads
// for any "ret4" return slot. We read it the same way.
constexpr std::uint16_t FRONTWINDOW_TOOL_SELECTOR = 0x150E;

}  // namespace

// Public entry-point on Agent: emit one window-list entry. Caller
// (the namespace-internal walker below) drives the iteration.
void Agent::emit_window_snapshot_entry(std::uint32_t addr,
                                       std::int16_t top,
                                       std::int16_t left,
                                       std::int16_t bottom,
                                       std::int16_t right,
                                       const std::uint8_t* title_chars,
                                       std::uint8_t title_len) {
    submit(protocol::build_window_snapshot_entry(
        addr, top, left, bottom, right, title_chars, title_len));
}

namespace {

// Walk the WindMgr's window list starting at `front`, emitting one
// TAG_WINDOW_SNAPSHOT_ENTRY per window. Bounded so a corrupt list
// can't infinite-loop us.
void walk_and_emit_window_snapshot(Agent& agent, MMU* mmu,
                                   std::uint32_t front) {
    if (mmu == nullptr) return;
    front &= 0x00FFFFFFu;
    if (front == 0) return;
    constexpr int MAX_WINDOWS = 64;
    std::uint32_t cur = front;
    for (int i = 0; i < MAX_WINDOWS && cur != 0; ++i) {
        // Sanity: bail if `cur` looks like uninitialized memory rather
        // than a real WindowPtr. Empirically WindMgr's list isn't always
        // NULL-terminated cleanly — Finder leaves the last window's
        // wNext field as heap fill bytes (e.g. $66666666). Catch the
        // common fill patterns (all 3 address bytes equal) and any
        // implausible bank > $0F (IIgs system heap lives in low banks).
        const std::uint8_t b0 = static_cast<std::uint8_t>(cur      );
        const std::uint8_t b1 = static_cast<std::uint8_t>(cur >>  8);
        const std::uint8_t b2 = static_cast<std::uint8_t>(cur >> 16);
        if (b2 > 0x0F) break;
        if (b0 == b1 && b1 == b2) break;

        // wStrucRgn handle → Region pointer → bounding rect.
        const std::uint32_t handle =
            read_le_long(mmu, cur + WINDREC_WSTRUCRGN_OFF) & 0x00FFFFFFu;
        std::int16_t top = 0, left = 0, bottom = 0, right = 0;
        if (handle != 0) {
            const std::uint32_t region_ptr =
                read_le_long(mmu, handle) & 0x00FFFFFFu;
            if (region_ptr != 0) {
                const std::uint32_t bbox = region_ptr + REGION_BBOX_OFFSET;
                top    = read_le_signed_word(mmu, bbox + 0);
                left   = read_le_signed_word(mmu, bbox + 2);
                bottom = read_le_signed_word(mmu, bbox + 4);
                right  = read_le_signed_word(mmu, bbox + 6);
            }
        }
        // Heuristic title scan over the WindRec memory.
        std::uint8_t title[MAX_TITLE_LEN];
        const std::uint8_t title_len = find_window_title(mmu, cur, title);

        std::fprintf(stderr,
                     "[agent] window snapshot: addr=$%06x bounds=(%d,%d)-(%d,%d) title=\"%.*s\"\n",
                     cur, left, top, right, bottom,
                     static_cast<int>(title_len),
                     reinterpret_cast<const char*>(title));
        agent.emit_window_snapshot_entry(cur, top, left, bottom, right,
                                         title, title_len);

        // Walk to next via wNext at WindowPtr - 4.
        const std::uint32_t next_addr =
            (cur + static_cast<std::uint32_t>(WINDREC_WNEXT_OFFSET))
                & 0x00FFFFFFu;
        const std::uint32_t next = read_le_long(mmu, next_addr) & 0x00FFFFFFu;
        if (next == cur || next == front) break;  // self-loop or cycle guard
        cur = next;
    }
}

}  // namespace

void Agent::on_tool_call_entry(cpu_state* cpu) {
    // Capture caller's args (16 bytes starting just past the JSL's
    // 3-byte return PC at SP+1..SP+3).
    std::uint8_t stack[protocol::TOOL_CALL_STACK_BYTES];
    read_stack(cpu->mmu, cpu->sp, /*offset=*/4,
               stack, protocol::TOOL_CALL_STACK_BYTES);

    submit(protocol::build_tool_call(cpu->x, cpu->a, cpu->y, cpu->sp,
                                     stack, protocol::TOOL_CALL_STACK_BYTES));

    // Opportunistic param-block dereference for tools whose first arg
    // is a pointer to a structure the receiver wants to inspect. Right
    // now: NewWindow ($090E) — paramListPtr at stack offset 0, points
    // at a ~76-byte WindParam record (wFrameBits, wTitle, wPosition,
    // …). 80 bytes covers it with a few bytes of slack.
    if (cpu->x == 0x090E && cpu->mmu != nullptr) {
        const std::uint32_t addr =
            (static_cast<std::uint32_t>(stack[0])) |
            (static_cast<std::uint32_t>(stack[1]) << 8) |
            (static_cast<std::uint32_t>(stack[2]) << 16);
        constexpr std::size_t blob_len = 80;
        std::uint8_t blob[blob_len];
        for (std::size_t i = 0; i < blob_len; ++i) {
            blob[i] = cpu->mmu->read((addr + i) & 0x00FFFFFFu);
        }
        submit(protocol::build_mem_blob(addr & 0x00FFFFFFu, blob, blob_len));

        // Second hop: wTitle is a 4-byte little-endian long pointer at
        // paramList offset 2..5. The string at that address is Pascal-
        // style: byte 0 = length, bytes 1..length = characters. Read
        // up to 256 bytes (length byte + 255 chars max). Note that
        // many windows (Finder folders especially) leave wTitle
        // pointing at uninitialized memory at NewWindow time and call
        // SetWTitle later — see the 0x0D0E hook below.
        const std::uint32_t wtitle_ptr =
            (static_cast<std::uint32_t>(blob[2])) |
            (static_cast<std::uint32_t>(blob[3]) << 8) |
            (static_cast<std::uint32_t>(blob[4]) << 16);
        if (wtitle_ptr != 0) {
            std::uint8_t title_blob[256];
            const std::uint8_t title_len =
                cpu->mmu->read(wtitle_ptr & 0x00FFFFFFu);
            title_blob[0] = title_len;
            for (std::size_t i = 0; i < title_len; ++i) {
                title_blob[1 + i] =
                    cpu->mmu->read((wtitle_ptr + 1u + i) & 0x00FFFFFFu);
            }
            submit(protocol::build_mem_blob(
                wtitle_ptr & 0x00FFFFFFu, title_blob,
                static_cast<std::size_t>(1) + title_len));
        }
    }

    // SetWTitle($0D0E) — caller already has the (now-allocated) title
    // string in real memory. Stack layout per the SetWTitle macro
    // (PxL ]1;]2): title pointer at offset 0 (last pushed), theWindow
    // at offset 4 (first pushed). Dereference the title and emit a
    // MEM_BLOB so the receiver can update the window's title.
    if (cpu->x == 0x0D0E && cpu->mmu != nullptr) {
        const std::uint32_t title_ptr =
            (static_cast<std::uint32_t>(stack[0])) |
            (static_cast<std::uint32_t>(stack[1]) << 8) |
            (static_cast<std::uint32_t>(stack[2]) << 16);
        if (title_ptr != 0) {
            std::uint8_t title_blob[256];
            const std::uint8_t title_len =
                cpu->mmu->read(title_ptr & 0x00FFFFFFu);
            title_blob[0] = title_len;
            for (std::size_t i = 0; i < title_len; ++i) {
                title_blob[1 + i] =
                    cpu->mmu->read((title_ptr + 1u + i) & 0x00FFFFFFu);
            }
            submit(protocol::build_mem_blob(
                title_ptr & 0x00FFFFFFu, title_blob,
                static_cast<std::size_t>(1) + title_len));
        }
    }

    // Compute the 24-bit address that RTL will jump to.
    // JSL pushes (return_PC - 1); RTL pulls and adds 1.
    std::uint8_t pcl = 0, pch = 0, pbr = 0;
    if (cpu->mmu != nullptr) {
        pcl = cpu->mmu->read(static_cast<std::uint16_t>(cpu->sp + 1u));
        pch = cpu->mmu->read(static_cast<std::uint16_t>(cpu->sp + 2u));
        pbr = cpu->mmu->read(static_cast<std::uint16_t>(cpu->sp + 3u));
    }
    const std::uint32_t pushed =
        (static_cast<std::uint32_t>(pbr) << 16) |
        (static_cast<std::uint32_t>(pch) << 8) |
        static_cast<std::uint32_t>(pcl);
    const std::uint32_t return_pc = (pushed + 1u) & 0x00FFFFFFu;

    // Push a frame so we can pair this entry with its exit. If we
    // overflow the small fixed stack, drop oldest frames silently —
    // it just means we miss return events for those calls.
    if (call_depth_ >= MAX_CALL_DEPTH) {
        // Shift down by one, lose the deepest frame. Rare; logging
        // would be more noise than help.
        for (std::size_t i = 1; i < MAX_CALL_DEPTH; ++i) {
            call_stack_[i - 1] = call_stack_[i];
        }
        --call_depth_;
    }
    call_stack_[call_depth_] = ToolCallFrame{
        return_pc, cpu->x, cpu->sp,
    };
    ++call_depth_;
    cpu->next_tool_return_pc = return_pc;
}

void Agent::on_tool_return(cpu_state* cpu) {
    if (call_depth_ == 0) {
        // Spurious match — shouldn't normally happen because the
        // sentinel value of next_tool_return_pc only matches when
        // there is at least one outstanding frame.
        cpu->next_tool_return_pc = 0xFFFFFFFFu;
        return;
    }
    const ToolCallFrame frame = call_stack_[call_depth_ - 1];
    --call_depth_;

    // The dispatcher pops the args and the JSL return PC before RTL,
    // so SP at exit is `sp_at_entry + 3 + arg_bytes`. We don't know
    // arg_bytes per-call; the simple thing is to read the same window
    // we did at entry, anchored to `sp_at_entry + 4`. The return slot
    // (now filled with the result) sits in those bytes regardless of
    // where the dispatcher left SP.
    std::uint8_t stack[protocol::TOOL_CALL_STACK_BYTES];
    read_stack(cpu->mmu, frame.sp_at_entry, /*offset=*/4,
               stack, protocol::TOOL_CALL_STACK_BYTES);

    submit(protocol::build_tool_return(frame.x_at_entry,
                                       cpu->a, cpu->y, frame.sp_at_entry,
                                       stack, protocol::TOOL_CALL_STACK_BYTES));

    // If a fresh compositor connection asked for a window-list snapshot,
    // wait for the next FrontWindow return — its result is the topmost
    // WindowPtr, which gives us the head of WindMgr's linked list. Walk
    // it via wNext at WindowPtr-4 and emit one TAG_WINDOW_SNAPSHOT_ENTRY
    // per window, then clear the flag. The IIgs's event loop calls
    // FrontWindow many times per second, so the wait is brief.
    if (frame.x_at_entry == FRONTWINDOW_TOOL_SELECTOR &&
        wants_window_snapshot_.load(std::memory_order_acquire)) {
        // FrontWindow has no args, so its 4-byte result slot sits at
        // sp_at_entry+4..+7 — exactly where `stack[]` starts. (Earlier
        // versions read from sp_at_entry+1, which is the JSL return-PC
        // bytes, and produced bogus WindowPtrs like $be<caller-PC>.)
        const std::uint32_t front =
            (static_cast<std::uint32_t>(stack[0])      ) |
            (static_cast<std::uint32_t>(stack[1]) <<  8) |
            (static_cast<std::uint32_t>(stack[2]) << 16) |
            (static_cast<std::uint32_t>(stack[3]) << 24);
        std::fprintf(stderr,
                     "[agent] FrontWindow=$%08x — emitting WindMgr snapshot\n",
                     front);
        walk_and_emit_window_snapshot(*this, cpu->mmu, front);
        wants_window_snapshot_.store(false, std::memory_order_release);
    }

    // Reset / restore the cpu's hot-path return-PC register.
    cpu->next_tool_return_pc = (call_depth_ > 0)
        ? call_stack_[call_depth_ - 1].return_pc
        : 0xFFFFFFFFu;
}

void Agent::io_thread_main() {
    // Outer loop: accept compositor → spawn reader → drain queue →
    // on disconnect, join reader and go back to accept.
    while (transport_->accept_blocking()) {
        // Greet the new connection. Built fresh per-connect so the receiver
        // always sees HELLO first, including across reconnects.
        const bool hello_ok = transport_->write_packet(protocol::build_hello(
                protocol::CAP_VBL | protocol::CAP_TOOLBOX |
                protocol::CAP_VIDEO_WRITES | protocol::CAP_INPUT_INJECTION));
        std::fprintf(stderr, "[agent] HELLO write %s\n",
                     hello_ok ? "ok" : "FAILED (peer already closed)");
        if (!hello_ok) {
            transport_->disconnect_client();
            continue;
        }

        // Right after HELLO, fire the post-connect callback if one is
        // registered. Used to push an initial-state snapshot (e.g. the
        // 32K SHR shadow) so a mid-run compositor doesn't have to wait
        // for the IIgs to repaint everything. Snapshot packets go onto
        // the queue ahead of any subsequent live VBL/MEM_WRITE events
        // (we're between the HELLO write and the queue-drain loop).
        std::function<void(Agent&)> cb;
        {
            std::lock_guard<std::mutex> lk(connect_cb_mu_);
            cb = on_client_connect_;
        }
        if (cb) {
            cb(*this);
        }

        // Spawn a reader thread for this client connection; it runs
        // until the client disconnects (read_exactly returns false),
        // then exits naturally and we join it below.
        std::thread reader([this] { input_reader_main(); });
        std::fprintf(stderr, "[agent] reader thread spawned\n");

        // Drain queue to this client until it disconnects or shutdown is
        // signaled.
        for (;;) {
            std::vector<std::uint8_t> packet;
            if (!queue_.pop_blocking(packet)) {
                // Queue shutdown. Final drain done; tear down reader,
                // then exit.
                transport_->disconnect_client();  // wakes reader's recv()
                if (reader.joinable()) reader.join();
                return;
            }
            if (!transport_->write_packet(packet)) {
                // Client gone. Disconnect + reap reader, back to accept.
                transport_->disconnect_client();
                if (reader.joinable()) reader.join();
                break;
            }
        }
    }
    // accept_blocking returned false → shutdown signaled.
}

void Agent::input_reader_main() {
    // Reads framed packets from the connected compositor and dispatches
    // them. Runs until the socket EOFs (returns false from
    // read_exactly), at which point the writer thread will join us.
    std::fprintf(stderr, "[agent] input reader thread started\n");
    for (;;) {
        std::uint8_t hdr[2];
        if (!transport_->read_exactly(hdr, 2)) {
            std::fprintf(stderr, "[agent] input reader: socket closed\n");
            return;
        }
        const std::uint16_t inner_len = (static_cast<std::uint16_t>(hdr[0]) << 8) |
                                         static_cast<std::uint16_t>(hdr[1]);
        if (inner_len == 0) {
            // Malformed; treat as disconnect.
            return;
        }
        std::vector<std::uint8_t> body(inner_len);
        if (!transport_->read_exactly(body.data(), inner_len)) return;

        const std::uint8_t tag = body[0];
        const std::uint8_t* payload = body.data() + 1;
        const std::size_t plen = body.size() - 1;

        // u16 / u8 helpers: big-endian on the wire.
        auto u16 = [&](std::size_t off) -> std::uint16_t {
            if (off + 2 > plen) return 0;
            return (static_cast<std::uint16_t>(payload[off]) << 8) |
                   static_cast<std::uint16_t>(payload[off + 1]);
        };

        switch (tag) {
            case protocol::TAG_INPUT_MOUSE_MOTION: {
                if (plen < 4) break;
                const std::uint16_t x = u16(0);
                const std::uint16_t y = u16(2);
                // The IIgs ADB mouse path consumes xrel/yrel only.
                // Compute deltas from our last injected position.
                const std::int32_t dx = static_cast<std::int32_t>(x) - last_inject_x_;
                const std::int32_t dy = static_cast<std::int32_t>(y) - last_inject_y_;
                last_inject_x_ = x;
                last_inject_y_ = y;

                SDL_Event ev;
                std::memset(&ev, 0, sizeof(ev));
                ev.type = SDL_EVENT_MOUSE_MOTION;
                ev.motion.timestamp = SDL_GetTicksNS();
                // Stamp .which with AGENT_MOUSEID so keygloo can tell
                // injected events apart from real host-OS mouse events
                // and apply the right per-mode filtering. Real mice get
                // OS-assigned IDs starting from 1; AGENT_MOUSEID is a
                // sentinel high value (see keygloo.hpp).
                ev.motion.which = protocol::AGENT_MOUSEID;
                ev.motion.x = static_cast<float>(x);
                ev.motion.y = static_cast<float>(y);
                ev.motion.xrel = static_cast<float>(dx);
                ev.motion.yrel = static_cast<float>(dy);
                const int rc = SDL_PushEvent(&ev);
                std::fprintf(stderr,
                             "[agent] input MOUSE_MOTION abs=(%u,%u) rel=(%d,%d) -> SDL_PushEvent=%d\n",
                             x, y, dx, dy, rc);
                break;
            }
            case protocol::TAG_INPUT_MOUSE_BUTTON: {
                if (plen < 6) break;
                const std::uint8_t button = payload[0];
                const std::uint8_t down = payload[1];
                const std::uint16_t x = u16(2);
                const std::uint16_t y = u16(4);
                SDL_Event ev;
                std::memset(&ev, 0, sizeof(ev));
                ev.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN
                               : SDL_EVENT_MOUSE_BUTTON_UP;
                ev.button.timestamp = SDL_GetTicksNS();
                ev.button.which = protocol::AGENT_MOUSEID;   // see comment above
                ev.button.button = button;
                ev.button.down = (down != 0);
                ev.button.clicks = 1;
                ev.button.x = static_cast<float>(x);
                ev.button.y = static_cast<float>(y);
                const int rc = SDL_PushEvent(&ev);
                std::fprintf(stderr,
                             "[agent] input MOUSE_BUTTON btn=%u down=%u (%u,%u) -> SDL_PushEvent=%d\n",
                             button, down, x, y, rc);
                break;
            }
            case protocol::TAG_INPUT_MOUSE_REL: {
                if (plen < 4) break;
                // Parse signed 16-bit big-endian.
                const std::int16_t dx = static_cast<std::int16_t>(
                    (static_cast<std::uint16_t>(payload[0]) << 8) |
                     static_cast<std::uint16_t>(payload[1]));
                const std::int16_t dy = static_cast<std::int16_t>(
                    (static_cast<std::uint16_t>(payload[2]) << 8) |
                     static_cast<std::uint16_t>(payload[3]));
                SDL_Event ev;
                std::memset(&ev, 0, sizeof(ev));
                ev.type = SDL_EVENT_MOUSE_MOTION;
                ev.motion.timestamp = SDL_GetTicksNS();
                ev.motion.which = protocol::AGENT_MOUSEID;
                // No abs coords; ADB_Mouse only consumes xrel/yrel.
                ev.motion.x = 0.0f;
                ev.motion.y = 0.0f;
                ev.motion.xrel = static_cast<float>(dx);
                ev.motion.yrel = static_cast<float>(dy);
                const int rc = SDL_PushEvent(&ev);
                std::fprintf(stderr,
                             "[agent] input MOUSE_REL rel=(%d,%d) -> SDL_PushEvent=%d\n",
                             dx, dy, rc);
                break;
            }
            case protocol::TAG_SET_MOUSE_MODE: {
                if (plen < 1) break;
                const std::uint8_t wire_mode = payload[0];
                // The reader thread can't safely touch keygloo state
                // directly, so we marshal the mode change onto the main
                // thread via a SDL_EVENT_USER. SDL_AppEvent in gs2.cpp
                // handles AGENT_USER_SET_MOUSE_MODE and calls
                // keygloo_set_mouse_mode().
                SDL_Event ev;
                std::memset(&ev, 0, sizeof(ev));
                ev.type = SDL_EVENT_USER;
                ev.user.timestamp = SDL_GetTicksNS();
                ev.user.code = protocol::AGENT_USER_SET_MOUSE_MODE;
                // Stash the mode in data1 as a small heap-free pointer-
                // sized integer cast. The handler reads it back as
                // uintptr_t.
                ev.user.data1 = reinterpret_cast<void*>(
                    static_cast<std::uintptr_t>(wire_mode));
                const int rc = SDL_PushEvent(&ev);
                std::fprintf(stderr,
                             "[agent] input SET_MOUSE_MODE mode=%u -> SDL_PushEvent=%d\n",
                             wire_mode, rc);
                break;
            }
            default:
                // Unknown tag — ignore but keep reading (forward compat).
                std::fprintf(stderr,
                             "[agent] unknown input tag 0x%02x len=%zu, ignoring\n",
                             tag, plen + 1);
                break;
        }
    }
}

}  // namespace agent
