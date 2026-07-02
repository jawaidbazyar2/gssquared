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

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

// Agent ↔ Compositor wire protocol.
//
// Framing on the wire:
//   [len: u16, big-endian]   length of (tag + payload), i.e. bytes following
//   [tag: u8]                packet type
//   [payload: len-1 bytes]   per-tag fields, big-endian
//
// Tag space partitions:
//   0x00 - 0x7F  agent → compositor
//   0x80 - 0xFF  compositor → agent
//
// All multi-byte integers are big-endian.

namespace agent::protocol {

constexpr std::uint8_t VERSION = 1;

// SDL identifiers used to bridge agent-injected events with the rest
// of the codebase. These live in the protocol header so both the agent
// (Agent.cpp) and the consumer side (devices/adb/keygloo.cpp) can refer
// to them without pulling in each other's headers.
//
// AGENT_MOUSEID — sentinel SDL_MouseID stamped onto agent-injected
// SDL_EVENT_MOUSE_* events so keygloo can tell them apart from real
// host events. Real mice get OS-assigned IDs starting at 1; SDL3
// reserves SDL_TOUCH_MOUSEID=0xFFFFFFFF and SDL_PEN_MOUSEID; we use
// 0xA9E97000 ("AGEN" + zeros) which collides with neither.
constexpr SDL_MouseID AGENT_MOUSEID = 0xA9E97000u;

// AGENT_USER_SET_MOUSE_MODE — code value placed on a SDL_EVENT_USER
// when the agent receives TAG_SET_MOUSE_MODE. The mode (0..2) goes in
// user.data1 cast to uintptr_t. SDL_AppEvent in gs2.cpp picks it up on
// the main thread and calls keygloo_set_mouse_mode().
constexpr Sint32 AGENT_USER_SET_MOUSE_MODE = 0x4147454D;  // 'AGEM'

// Capability bits advertised in HELLO. Each bit means "this agent emits
// packets of this category." Compositor uses these to decide which features
// it can offer the user.
constexpr std::uint32_t CAP_VBL              = 0x0001;
constexpr std::uint32_t CAP_TOOLBOX          = 0x0002;
constexpr std::uint32_t CAP_VIDEO_WRITES     = 0x0004;
constexpr std::uint32_t CAP_INPUT_INJECTION  = 0x0008;  // future

enum Tag : std::uint8_t {
    // -- agent → compositor (0x00-0x7F) --
    TAG_HELLO       = 0x00,
    // One frame tick per IIgs video frame, emitted at the start of the
    // VBL period (scanline 192 in the GSSquared scanner). Used by the
    // compositor as a "render now" cadence signal. We don't emit a separate
    // VBL_END; one tick per frame is enough for compositing purposes.
    TAG_VBL         = 0x01,
    // IIgs Toolbox dispatcher entry. Emitted when execution reaches
    // $E10000 (the Toolbox dispatcher). Payload:
    //   [u16 X][u16 A][u16 Y]                — registers at entry
    //   [u16 SP][u8 stack_len][N bytes]      — top N bytes of stack
    //                                          starting at SP+4 (i.e.
    //                                          caller's args, after
    //                                          the JSL's 3-byte return
    //                                          PC). N is currently 16.
    // X holds the IIgs Toolbox call selector (high byte = function,
    // low byte = tool set). The stack bytes carry the args the caller
    // pushed; receivers decode them per a per-tool signature table.
    // We do NOT yet emit a corresponding return event.
    TAG_TOOL_CALL   = 0x02,
    // CPU write that lands in IIgs video memory or video softswitches.
    // Payload: [u32 BE address (24-bit, top byte 0)] [u8 data].
    // Address is normalized to where the write is observable on the slow
    // bus: $E0xxxx for text/hires/double-hires backing, $E1xxxx for the
    // SHR buffer, $C0xx for softswitches. Writes that came in via main-bank
    // ($00/$01) shadow are translated to their $E0/$E1 mirror address
    // before emission, so the receiver only ever sees the canonical
    // video-memory view.
    TAG_MEM_WRITE   = 0x03,
    // Toolbox dispatcher exit. Emitted when execution reaches the
    // saved return PC of an in-flight tool call, after the tool has
    // run. Same payload shape as TOOL_CALL — A/Y are the registers at
    // exit, X carries the *original* selector saved at entry, the
    // stack capture covers the return slot (now filled by the tool)
    // plus the popped args.
    TAG_TOOL_RETURN = 0x04,
    // Opportunistic memory-block dump emitted by the agent when a hook
    // wants the receiver to see structures pointed at by a register/arg.
    // First user: NewWindow emits the paramList block (window position,
    // title pointer, refcon, etc.) so the receiver can extract the
    // window's bounds without a follow-up request. Payload:
    //   [u32 addr (24-bit)] [u16 len] [len bytes]
    // Receivers associate by *immediate ordering* — a MEM_BLOB
    // emitted right after a TOOL_CALL belongs to that call.
    TAG_MEM_BLOB    = 0x05,
    // Per-window snapshot entry, fired by the agent on a new
    // compositor connection so it can populate panels for windows
    // that existed before the compositor attached. The agent walks
    // the IIgs Window Manager's window list (via wNext at offset
    // -4 from each WindowPtr, head from the next observed
    // FrontWindow return) and emits one of these per window.
    // Payload:
    //   [u32 windowAddr][s16 top][s16 left][s16 bottom][s16 right]
    // Bounds are the wStrucRgn bbox (frame + content), in 640-mode
    // screen coordinates; matches what NewWindow's paramList
    // wPosition would carry.
    TAG_WINDOW_SNAPSHOT_ENTRY = 0x06,

    // -- compositor → agent (0x80-0xFF) --
    // Input injection. The agent receives these from the connected
    // compositor and synthesizes corresponding SDL_Events via
    // SDL_PushEvent so keygloo (the existing ADB handler) picks them
    // up as if the host user had typed/clicked.
    //
    // TAG_INPUT_MOUSE_MOTION payload: [u16 x][u16 y]   (big-endian)
    // TAG_INPUT_MOUSE_BUTTON payload: [u8 button][u8 down(1=press, 0=release)][u16 x][u16 y]
    //   Button numbers are SDL3 conventions: 1=left, 2=middle, 3=right.
    TAG_INPUT_MOUSE_MOTION = 0x80,
    TAG_INPUT_MOUSE_BUTTON = 0x81,
    // Set the IIgs's mouse-input mode. The agent doesn't change keygloo
    // state directly (its reader thread is decoupled from the main
    // SDL/event thread); on receipt it pushes a SDL_EVENT_USER with
    // code AGENT_USER_SET_MOUSE_MODE so the main thread can call
    // keygloo_set_mouse_mode().
    // Payload: [u8 mode]   0=FOLLOW_HOST, 1=CAPTURE, 2=DISABLED
    TAG_SET_MOUSE_MODE     = 0x82,
    // Relative mouse motion. The agent forwards (dx, dy) directly as
    // an SDL_EVENT_MOUSE_MOTION's xrel/yrel — no last_inject tracking,
    // no absolute math. This avoids the "stale agent baseline" trap
    // where TAG_INPUT_MOUSE_MOTION's absolute-coord model produces
    // huge ghost deltas across reconnects (e.g., a fresh client whose
    // first abs=(0,0) collides with an agent whose last_inject is
    // somewhere near 32000). For new clients, prefer this packet.
    // Payload: [s16 dx][s16 dy]   (signed; big-endian)
    TAG_INPUT_MOUSE_REL    = 0x83,
};

// Wire values for TAG_SET_MOUSE_MODE. These mirror MouseMode in
// keygloo.hpp but live here so the protocol layer doesn't depend on
// keygloo internals.
constexpr std::uint8_t MOUSE_MODE_WIRE_FOLLOW_HOST = 0;
constexpr std::uint8_t MOUSE_MODE_WIRE_CAPTURE     = 1;
constexpr std::uint8_t MOUSE_MODE_WIRE_DISABLED    = 2;

// Big-endian encoding helpers — public so the IO thread (or stub) can use
// them when stitching together packets that don't deserve a builder.
void put_u16_be(std::vector<std::uint8_t>& buf, std::uint16_t v);
void put_u32_be(std::vector<std::uint8_t>& buf, std::uint32_t v);

// Packet builders. Each returns a fully-framed packet (length prefix + tag
// + payload) ready to write to the wire.
std::vector<std::uint8_t> build_hello(std::uint32_t capabilities);
std::vector<std::uint8_t> build_vbl(std::uint32_t frame_seq);
// Number of stack bytes captured per Toolbox call. Sized to cover the
// common case of up to four 4-byte pointer args, plus a 4-byte return
// slot. Tools with longer signatures will be decoded partially today
// and we'll widen this if necessary.
constexpr std::size_t TOOL_CALL_STACK_BYTES = 16;

std::vector<std::uint8_t> build_tool_call(std::uint16_t x_reg,
                                          std::uint16_t a_reg,
                                          std::uint16_t y_reg,
                                          std::uint16_t sp,
                                          const std::uint8_t* stack_bytes,
                                          std::size_t stack_len);
std::vector<std::uint8_t> build_tool_return(std::uint16_t x_reg,
                                            std::uint16_t a_reg,
                                            std::uint16_t y_reg,
                                            std::uint16_t sp,
                                            const std::uint8_t* stack_bytes,
                                            std::size_t stack_len);
std::vector<std::uint8_t> build_mem_write(std::uint32_t addr_24bit,
                                          std::uint8_t data);
std::vector<std::uint8_t> build_mem_blob(std::uint32_t addr_24bit,
                                         const std::uint8_t* data,
                                         std::size_t len);
std::vector<std::uint8_t> build_set_mouse_mode(std::uint8_t mode);
std::vector<std::uint8_t> build_window_snapshot_entry(
    std::uint32_t window_addr,
    std::int16_t top, std::int16_t left,
    std::int16_t bottom, std::int16_t right,
    const std::uint8_t* title_chars,
    std::uint8_t title_len);

// Human-readable rendering of a framed packet, used by the stub transport
// and for debug logging. Returns something like "VBL seq=42".
std::string describe(const std::vector<std::uint8_t>& packet);

}  // namespace agent::protocol
