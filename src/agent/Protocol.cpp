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

#include "agent/Protocol.hpp"

#include <cstdio>
#include <cstring>

namespace agent::protocol {

void put_u16_be(std::vector<std::uint8_t>& buf, std::uint16_t v) {
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void put_u32_be(std::vector<std::uint8_t>& buf, std::uint32_t v) {
    buf.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

namespace {

// Build a framed packet from a tag and a payload-builder lambda. Reserves the
// length prefix slot, calls the builder to fill in payload bytes, then patches
// the length in.
template <typename PayloadFn>
std::vector<std::uint8_t> framed(Tag tag, PayloadFn&& fill_payload) {
    std::vector<std::uint8_t> pkt;
    pkt.reserve(16);
    pkt.push_back(0);                 // length high byte (placeholder)
    pkt.push_back(0);                 // length low byte (placeholder)
    pkt.push_back(static_cast<std::uint8_t>(tag));
    fill_payload(pkt);
    const std::size_t inner_len = pkt.size() - 2;  // tag + payload bytes
    pkt[0] = static_cast<std::uint8_t>((inner_len >> 8) & 0xFF);
    pkt[1] = static_cast<std::uint8_t>(inner_len & 0xFF);
    return pkt;
}

}  // namespace

std::vector<std::uint8_t> build_hello(std::uint32_t capabilities) {
    return framed(TAG_HELLO, [&](std::vector<std::uint8_t>& p) {
        p.push_back(VERSION);
        put_u32_be(p, capabilities);
    });
}

std::vector<std::uint8_t> build_vbl(std::uint32_t frame_seq) {
    return framed(TAG_VBL, [&](std::vector<std::uint8_t>& p) {
        put_u32_be(p, frame_seq);
    });
}

std::vector<std::uint8_t> build_tool_call(std::uint16_t x_reg,
                                          std::uint16_t a_reg,
                                          std::uint16_t y_reg,
                                          std::uint16_t sp,
                                          const std::uint8_t* stack_bytes,
                                          std::size_t stack_len) {
    return framed(TAG_TOOL_CALL, [&](std::vector<std::uint8_t>& p) {
        put_u16_be(p, x_reg);
        put_u16_be(p, a_reg);
        put_u16_be(p, y_reg);
        put_u16_be(p, sp);
        p.push_back(static_cast<std::uint8_t>(stack_len));
        for (std::size_t i = 0; i < stack_len; ++i) {
            p.push_back(stack_bytes[i]);
        }
    });
}

std::vector<std::uint8_t> build_tool_return(std::uint16_t x_reg,
                                            std::uint16_t a_reg,
                                            std::uint16_t y_reg,
                                            std::uint16_t sp,
                                            const std::uint8_t* stack_bytes,
                                            std::size_t stack_len) {
    return framed(TAG_TOOL_RETURN, [&](std::vector<std::uint8_t>& p) {
        put_u16_be(p, x_reg);
        put_u16_be(p, a_reg);
        put_u16_be(p, y_reg);
        put_u16_be(p, sp);
        p.push_back(static_cast<std::uint8_t>(stack_len));
        for (std::size_t i = 0; i < stack_len; ++i) {
            p.push_back(stack_bytes[i]);
        }
    });
}

std::vector<std::uint8_t> build_mem_write(std::uint32_t addr_24bit,
                                          std::uint8_t data) {
    return framed(TAG_MEM_WRITE, [&](std::vector<std::uint8_t>& p) {
        put_u32_be(p, addr_24bit & 0x00FFFFFFu);
        p.push_back(data);
    });
}

std::vector<std::uint8_t> build_mem_blob(std::uint32_t addr_24bit,
                                         const std::uint8_t* data,
                                         std::size_t len) {
    return framed(TAG_MEM_BLOB, [&](std::vector<std::uint8_t>& p) {
        put_u32_be(p, addr_24bit & 0x00FFFFFFu);
        put_u16_be(p, static_cast<std::uint16_t>(len));
        for (std::size_t i = 0; i < len; ++i) {
            p.push_back(data[i]);
        }
    });
}

std::vector<std::uint8_t> build_set_mouse_mode(std::uint8_t mode) {
    return framed(TAG_SET_MOUSE_MODE, [&](std::vector<std::uint8_t>& p) {
        p.push_back(mode);
    });
}

std::vector<std::uint8_t> build_window_snapshot_entry(
    std::uint32_t window_addr,
    std::int16_t top, std::int16_t left,
    std::int16_t bottom, std::int16_t right,
    const std::uint8_t* title_chars,
    std::uint8_t title_len) {
    return framed(TAG_WINDOW_SNAPSHOT_ENTRY, [&](std::vector<std::uint8_t>& p) {
        put_u32_be(p, window_addr & 0x00FFFFFFu);
        put_u16_be(p, static_cast<std::uint16_t>(top));
        put_u16_be(p, static_cast<std::uint16_t>(left));
        put_u16_be(p, static_cast<std::uint16_t>(bottom));
        put_u16_be(p, static_cast<std::uint16_t>(right));
        // Title trailer: [u8 len][len bytes]. len=0 means no title found.
        // Title bytes are MacRoman chars (no Pascal length prefix).
        p.push_back(title_len);
        for (std::uint8_t i = 0; i < title_len; ++i) {
            p.push_back(title_chars[i]);
        }
    });
}

std::string describe(const std::vector<std::uint8_t>& packet) {
    if (packet.size() < 3) {
        return "short-packet";
    }
    const std::uint8_t tag = packet[2];
    char buf[64];
    auto seq32 = [&packet]() -> std::uint32_t {
        if (packet.size() < 7) return 0;
        return (static_cast<std::uint32_t>(packet[3]) << 24) |
               (static_cast<std::uint32_t>(packet[4]) << 16) |
               (static_cast<std::uint32_t>(packet[5]) << 8) |
               (static_cast<std::uint32_t>(packet[6]));
    };
    switch (tag) {
        case TAG_HELLO: {
            // HELLO payload: [u8 version][u32 capabilities] starting at index 3.
            const std::uint8_t version = packet.size() >= 4 ? packet[3] : 0;
            std::uint32_t caps = 0;
            if (packet.size() >= 8) {
                caps = (static_cast<std::uint32_t>(packet[4]) << 24) |
                       (static_cast<std::uint32_t>(packet[5]) << 16) |
                       (static_cast<std::uint32_t>(packet[6]) << 8) |
                       (static_cast<std::uint32_t>(packet[7]));
            }
            std::snprintf(buf, sizeof(buf), "HELLO version=%u caps=0x%08x",
                          version, static_cast<unsigned>(caps));
            return buf;
        }
        case TAG_VBL:
            std::snprintf(buf, sizeof(buf), "VBL seq=%u", seq32());
            return buf;
        case TAG_MEM_WRITE: {
            // Payload: u32 addr, u8 data starting at index 3.
            if (packet.size() < 8) return "MEM_WRITE short";
            std::uint32_t addr =
                (static_cast<std::uint32_t>(packet[3]) << 24) |
                (static_cast<std::uint32_t>(packet[4]) << 16) |
                (static_cast<std::uint32_t>(packet[5]) << 8) |
                (static_cast<std::uint32_t>(packet[6]));
            const std::uint8_t data = packet[7];
            std::snprintf(buf, sizeof(buf),
                          "MEM_WRITE addr=%06x data=%02x",
                          static_cast<unsigned>(addr & 0x00FFFFFFu),
                          static_cast<unsigned>(data));
            return buf;
        }
        case TAG_TOOL_CALL:
        case TAG_TOOL_RETURN: {
            // Payload: u16 X, u16 A, u16 Y, u16 SP, u8 stack_len, N stack bytes
            auto u16 = [&packet](std::size_t off) -> std::uint16_t {
                if (packet.size() < off + 2) return 0;
                return (static_cast<std::uint16_t>(packet[off]) << 8) |
                       static_cast<std::uint16_t>(packet[off + 1]);
            };
            const std::uint16_t x = u16(3);
            const std::uint16_t a = u16(5);
            const std::uint16_t y = u16(7);
            const std::uint16_t sp = u16(9);
            const std::size_t slen =
                packet.size() > 11 ? packet[11] : 0;
            std::snprintf(buf, sizeof(buf),
                          "%s x=%04x a=%04x y=%04x sp=%04x +%zu",
                          tag == TAG_TOOL_CALL ? "TOOL_CALL" : "TOOL_RETURN",
                          x, a, y, sp, slen);
            return buf;
        }
        default:
            std::snprintf(buf, sizeof(buf), "UNKNOWN tag=0x%02x len=%zu", tag, packet.size());
            return buf;
    }
}

}  // namespace agent::protocol
