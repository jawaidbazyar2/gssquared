/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   W5100 register/memory model for Uthernet II.
 *   Register semantics adapted from AppleWin Uthernet2.cpp (Andrea Odetti, GPL).
 */

#include "w5100_chip.hpp"

#include <cstdio>
#include <cstring>

namespace u2 {
namespace {

uint8_t get_ibyte(uint16_t value, size_t shift) {
    return static_cast<uint8_t>((value >> shift) & 0xFF);
}

}  // namespace

W5100Chip::W5100Chip() {
    memory_.assign(W5100_MEM_SIZE, 0);
}

W5100Chip::~W5100Chip() {
    stop();
}

bool W5100Chip::start() {
    reset(true);
    return worker_.start();
}

void W5100Chip::stop() {
    worker_.stop();
}

void W5100Chip::reset(bool power_cycle) {
    mode_ = 0;
    if (power_cycle) {
        data_addr_ = 0;
    }

    // Close any host sockets (quiet — reset already sets CLOSED locally)
    for (uint8_t i = 0; i < U2_NUM_SOCKETS; ++i) {
        NetRequest req;
        req.msg = NetMsg::CloseHost;
        req.sock = i;
        worker_.post(std::move(req));
    }

    memory_.assign(W5100_MEM_SIZE, 0);
    for (size_t i = 0; i < sockets_.size(); ++i) {
        sockets_[i] = SocketState{};
        sockets_[i].registerAddress = static_cast<uint16_t>(W5100_S0_BASE + (i << 8));
        reset_rxtx(i);
        const uint16_t ra = sockets_[i].registerAddress;
        memory_[ra + W5100_SN_DHAR0] = 0xFF;
        memory_[ra + W5100_SN_DHAR1] = 0xFF;
        memory_[ra + W5100_SN_DHAR2] = 0xFF;
        memory_[ra + W5100_SN_DHAR3] = 0xFF;
        memory_[ra + W5100_SN_DHAR4] = 0xFF;
        memory_[ra + W5100_SN_DHAR5] = 0xFF;
        memory_[ra + W5100_SN_TTL] = 0x80;
        sockets_[i].status = W5100_SN_SR_CLOSED;
    }
    memory_[W5100_RTR0] = 0x07;
    memory_[W5100_RTR1] = 0xD0;
    memory_[W5100_RCR] = 0x08;
    memory_[W5100_PTIMER] = 0x28;
    set_rx_sizes(0x55);
    set_tx_sizes(0x55);
}

void W5100Chip::set_tx_sizes(uint8_t value) {
    memory_[W5100_TMSR] = value;
    uint16_t base = W5100_TX_BASE;
    const uint16_t end = W5100_RX_BASE;
    for (auto &socket : sockets_) {
        socket.transmitBase = base;
        const uint8_t bits = value & 0x03;
        value >>= 2;
        const uint16_t size = static_cast<uint16_t>(1u << (10 + bits));
        base = static_cast<uint16_t>(base + size);
        if (base > end) {
            base = end;
        }
        socket.transmitSize = static_cast<uint16_t>(base - socket.transmitBase);
    }
}

void W5100Chip::set_rx_sizes(uint8_t value) {
    memory_[W5100_RMSR] = value;
    uint16_t base = W5100_RX_BASE;
    const uint16_t end = W5100_MEM_SIZE;
    for (auto &socket : sockets_) {
        socket.receiveBase = base;
        const uint8_t bits = value & 0x03;
        value >>= 2;
        const uint16_t size = static_cast<uint16_t>(1u << (10 + bits));
        base = static_cast<uint16_t>(base + size);
        if (base > end) {
            base = end;
        }
        socket.receiveSize = static_cast<uint16_t>(base - socket.receiveBase);
    }
}

void W5100Chip::reset_rxtx(size_t i) {
    auto &socket = sockets_[i];
    socket.sn_rx_wr = 0;
    socket.sn_rx_rsr = 0;
    memory_[socket.registerAddress + W5100_SN_TX_RD0] = 0;
    memory_[socket.registerAddress + W5100_SN_TX_RD1] = 0;
    memory_[socket.registerAddress + W5100_SN_TX_WR0] = 0;
    memory_[socket.registerAddress + W5100_SN_TX_WR1] = 0;
    memory_[socket.registerAddress + W5100_SN_RX_RD0] = 0;
    memory_[socket.registerAddress + W5100_SN_RX_RD1] = 0;
}

void W5100Chip::set_header_size(size_t i) {
    switch (sockets_[i].status) {
    case W5100_SN_SR_ESTABLISHED:
        sockets_[i].headerSize = 0;
        break;
    case W5100_SN_SR_SOCK_UDP:
        sockets_[i].headerSize = 8; // IP + Port + Size
        break;
    case W5100_SN_SR_SOCK_IPRAW:
        sockets_[i].headerSize = 6; // IP + Size
        break;
    case W5100_SN_SR_SOCK_MACRAW:
        sockets_[i].headerSize = 2;
        break;
    default:
        sockets_[i].headerSize = 0;
        break;
    }
}

void W5100Chip::update_rsr(size_t i) {
    auto &socket = sockets_[i];
    const int size = socket.receiveSize;
    const uint16_t mask = static_cast<uint16_t>(size - 1);
    const int sn_rx_rd = read_be16(&memory_[socket.registerAddress + W5100_SN_RX_RD0]) & mask;
    const int sn_rx_wr = socket.sn_rx_wr & mask;
    int dataPresent = sn_rx_wr - sn_rx_rd;
    if (dataPresent < 0) {
        dataPresent += size;
    }
    socket.sn_rx_rsr = static_cast<uint16_t>(dataPresent);
}

uint16_t W5100Chip::tx_data_size(size_t i) const {
    const auto &socket = sockets_[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = static_cast<uint16_t>(size - 1);
    const int sn_tx_rd = read_be16(&memory_[socket.registerAddress + W5100_SN_TX_RD0]) & mask;
    const int sn_tx_wr = read_be16(&memory_[socket.registerAddress + W5100_SN_TX_WR0]) & mask;
    int present = sn_tx_wr - sn_tx_rd;
    if (present < 0) {
        present += size;
    }
    return static_cast<uint16_t>(present);
}

uint8_t W5100Chip::tx_free_reg(size_t i, size_t shift) const {
    const int size = sockets_[i].transmitSize;
    const uint16_t free = static_cast<uint16_t>(size - tx_data_size(i));
    return get_ibyte(free, shift);
}

uint8_t W5100Chip::rx_size_reg(size_t i, size_t shift) const {
    return get_ibyte(sockets_[i].sn_rx_rsr, shift);
}

bool W5100Chip::room_for(size_t i, size_t len) const {
    const auto &s = sockets_[i];
    return s.sn_rx_rsr + len + s.headerSize < s.receiveSize;
}

void W5100Chip::write_rx_byte(size_t i, uint8_t v) {
    auto &socket = sockets_[i];
    const uint16_t base = socket.receiveBase;
    memory_[base + socket.sn_rx_wr] = v;
    socket.sn_rx_wr = static_cast<uint16_t>((socket.sn_rx_wr + 1) % socket.receiveSize);
    ++socket.sn_rx_rsr;
}

void W5100Chip::write_rx_be16(size_t i, uint16_t v) {
    write_rx_byte(i, get_ibyte(v, 8));
    write_rx_byte(i, get_ibyte(v, 0));
}

void W5100Chip::write_rx_data(size_t i, const uint8_t *data, size_t len) {
    for (size_t c = 0; c < len; ++c) {
        write_rx_byte(i, data[c]);
    }
}

void W5100Chip::drain_events() {
    NetEvent ev;
    while (worker_.poll_event(&ev)) {
        apply_event(ev);
    }
}

void W5100Chip::apply_event(const NetEvent &ev) {
    if (ev.sock >= U2_NUM_SOCKETS && ev.evt != NetEvt::MacRawRx) {
        return;
    }
    switch (ev.evt) {
    case NetEvt::Status:
        sockets_[ev.sock].status = ev.status;
        set_header_size(ev.sock);
        break;
    case NetEvt::RxTcp:
        if (room_for(ev.sock, ev.payload.size())) {
            write_rx_data(ev.sock, ev.payload.data(), ev.payload.size());
        }
        break;
    case NetEvt::RxUdp:
        if (room_for(ev.sock, ev.payload.size())) {
            // W5100 UDP header: dest IP (4) + port (2) + size (2) then payload
            const uint8_t *ip = reinterpret_cast<const uint8_t *>(&ev.src_ip);
            write_rx_byte(ev.sock, ip[0]);
            write_rx_byte(ev.sock, ip[1]);
            write_rx_byte(ev.sock, ip[2]);
            write_rx_byte(ev.sock, ip[3]);
            write_rx_be16(ev.sock, ev.src_port);
            write_rx_be16(ev.sock, static_cast<uint16_t>(ev.payload.size()));
            write_rx_data(ev.sock, ev.payload.data(), ev.payload.size());
        }
        break;
    case NetEvt::MacRawRx:
        ingest_macraw(ev);
        break;
    case NetEvt::IpRawRx:
        if (room_for(ev.sock, ev.payload.size())) {
            const uint8_t *ip = reinterpret_cast<const uint8_t *>(&ev.src_ip);
            write_rx_byte(ev.sock, ip[0]);
            write_rx_byte(ev.sock, ip[1]);
            write_rx_byte(ev.sock, ip[2]);
            write_rx_byte(ev.sock, ip[3]);
            write_rx_be16(ev.sock, static_cast<uint16_t>(ev.payload.size()));
            write_rx_data(ev.sock, ev.payload.data(), ev.payload.size());
        }
        break;
    default:
        break;
    }
}

void W5100Chip::ingest_macraw(const NetEvent &ev) {
    if (ev.payload.size() < ETH_MINIMUM_SIZE) {
        return;
    }
    const uint8_t *frame = ev.payload.data();
    const uint8_t *mac = &memory_[W5100_SHAR0];
    const bool to_us = (frame[0] == mac[0] && frame[1] == mac[1] && frame[2] == mac[2] &&
                        frame[3] == mac[3] && frame[4] == mac[4] && frame[5] == mac[5]);
    const bool bcast = (frame[0] == 0xFF && frame[1] == 0xFF && frame[2] == 0xFF &&
                        frame[3] == 0xFF && frame[4] == 0xFF && frame[5] == 0xFF);

    // Prefer IPRAW sockets matching protocol if frame is IPv4
    if ((to_us || bcast) && frame[12] == 0x08 && frame[13] == 0x00 && ev.payload.size() >= 34) {
        const uint8_t proto = frame[23];
        const uint32_t src_ip = *reinterpret_cast<const uint32_t *>(frame + 26);
        const size_t ip_hdr = (frame[14] & 0x0F) * 4;
        if (14 + ip_hdr <= ev.payload.size()) {
            const uint8_t *payload = frame + 14 + ip_hdr;
            const size_t plen = ev.payload.size() - (14 + ip_hdr);
            for (size_t i = 0; i < sockets_.size(); ++i) {
                if (sockets_[i].status != W5100_SN_SR_SOCK_IPRAW) {
                    continue;
                }
                if (memory_[sockets_[i].registerAddress + W5100_SN_PROTO] != proto) {
                    continue;
                }
                if (!room_for(i, plen)) {
                    break;
                }
                const uint8_t *ipb = reinterpret_cast<const uint8_t *>(&src_ip);
                write_rx_byte(i, ipb[0]);
                write_rx_byte(i, ipb[1]);
                write_rx_byte(i, ipb[2]);
                write_rx_byte(i, ipb[3]);
                write_rx_be16(i, static_cast<uint16_t>(plen));
                write_rx_data(i, payload, plen);
                return;
            }
        }
    }

    // MACRAW only on socket 0
    if (sockets_[0].status != W5100_SN_SR_SOCK_MACRAW) {
        return;
    }
    const uint8_t mr = memory_[sockets_[0].registerAddress + W5100_SN_MR];
    const bool filter = (mr & W5100_SN_MR_MF) != 0;
    if (filter && !to_us && !bcast) {
        return;
    }
    if (!room_for(0, ev.payload.size())) {
        return;
    }
    // W5100 MACRAW header is big-endian length including the 2-byte header itself
    // (same as AppleWin writeDataMacRaw). Contiki subtracts 2 after reading it.
    write_rx_be16(0, static_cast<uint16_t>(ev.payload.size() + 2));
    write_rx_data(0, frame, ev.payload.size());
}

void W5100Chip::open_socket(size_t i) {
    auto &socket = sockets_[i];
    const uint8_t mr = memory_[socket.registerAddress + W5100_SN_MR];
    const uint8_t protocol = mr & W5100_SN_MR_PROTO_MASK;

    // Drop any prior host socket without a Status event — a CLOSE→CLOSED
    // completion would otherwise clobber MACRAW/IPRAW (and race TCP/UDP open).
    NetRequest close_req;
    close_req.msg = NetMsg::CloseHost;
    close_req.sock = static_cast<uint8_t>(i);
    worker_.post(std::move(close_req));

    switch (protocol) {
    case W5100_SN_MR_IPRAW:
        socket.status = W5100_SN_SR_SOCK_IPRAW;
        set_header_size(i);
        break;
    case W5100_SN_MR_MACRAW:
        socket.status = W5100_SN_SR_SOCK_MACRAW;
        set_header_size(i);
        break;
    case W5100_SN_MR_TCP: {
        NetRequest req;
        req.msg = NetMsg::OpenTcp;
        req.sock = static_cast<uint8_t>(i);
        worker_.post(std::move(req));
        socket.status = W5100_SN_SR_SOCK_INIT;
        set_header_size(i);
        break;
    }
    case W5100_SN_MR_UDP: {
        NetRequest req;
        req.msg = NetMsg::OpenUdp;
        req.sock = static_cast<uint8_t>(i);
        req.local_port = read_be16(&memory_[socket.registerAddress + W5100_SN_PORT0]);
        worker_.post(std::move(req));
        socket.status = W5100_SN_SR_SOCK_UDP;
        set_header_size(i);
        break;
    }
    default:
        socket.status = W5100_SN_SR_CLOSED;
        break;
    }
    reset_rxtx(i);
}

void W5100Chip::send_data(size_t i) {
    auto &socket = sockets_[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = static_cast<uint16_t>(size - 1);
    const uint16_t sn_tx_rd = read_be16(&memory_[socket.registerAddress + W5100_SN_TX_RD0]) & mask;
    const uint16_t sn_tx_wr = read_be16(&memory_[socket.registerAddress + W5100_SN_TX_WR0]) & mask;
    int present = sn_tx_wr - sn_tx_rd;
    if (present < 0) {
        present += size;
    }
    if (present <= 0) {
        return;
    }

    std::vector<uint8_t> packet(static_cast<size_t>(present));
    for (int c = 0; c < present; ++c) {
        packet[static_cast<size_t>(c)] =
            memory_[socket.transmitBase + ((sn_tx_rd + c) & mask)];
    }

    // Advance TX_RD to TX_WR (consumed)
    memory_[socket.registerAddress + W5100_SN_TX_RD0] = memory_[socket.registerAddress + W5100_SN_TX_WR0];
    memory_[socket.registerAddress + W5100_SN_TX_RD1] = memory_[socket.registerAddress + W5100_SN_TX_WR1];

    NetRequest req;
    req.sock = static_cast<uint8_t>(i);
    req.payload = std::move(packet);

    switch (socket.status) {
    case W5100_SN_SR_ESTABLISHED:
        req.msg = NetMsg::SendTcp;
        worker_.post(std::move(req));
        break;
    case W5100_SN_SR_SOCK_UDP:
        req.msg = NetMsg::SendUdp;
        req.dest_ip = *reinterpret_cast<uint32_t *>(&memory_[socket.registerAddress + W5100_SN_DIPR0]);
        req.dest_port = read_be16(&memory_[socket.registerAddress + W5100_SN_DPORT0]);
        worker_.post(std::move(req));
        break;
    case W5100_SN_SR_SOCK_MACRAW:
        req.msg = NetMsg::MacRawTx;
        worker_.post(std::move(req));
        break;
    case W5100_SN_SR_SOCK_IPRAW: {
        // Build a minimal Ethernet+IPv4 frame for slirp (AppleWin sends via NetworkBackend).
        // Guest TX buffer is IP payload only; prepend IP header fields from registers is complex.
        // For IPRAW, AppleWin constructs IP packet — here we treat payload as full ethernet if large
        // enough, else wrap as IPv4 datagram with dest from DIPR and proto from SN_PROTO.
        std::vector<uint8_t> frame;
        frame.resize(14 + 20 + req.payload.size());
        // Dest MAC broadcast; src from SHAR
        std::memset(frame.data(), 0xFF, 6);
        std::memcpy(frame.data() + 6, &memory_[W5100_SHAR0], 6);
        frame[12] = 0x08;
        frame[13] = 0x00;
        uint8_t *ip = frame.data() + 14;
        ip[0] = 0x45;
        ip[1] = 0;
        const uint16_t tot = static_cast<uint16_t>(20 + req.payload.size());
        ip[2] = static_cast<uint8_t>(tot >> 8);
        ip[3] = static_cast<uint8_t>(tot & 0xFF);
        ip[8] = memory_[socket.registerAddress + W5100_SN_TTL];
        ip[9] = memory_[socket.registerAddress + W5100_SN_PROTO];
        std::memcpy(ip + 12, &memory_[W5100_SIPR0], 4);
        std::memcpy(ip + 16, &memory_[socket.registerAddress + W5100_SN_DIPR0], 4);
        // checksum left 0 — slirp/stack may still accept for some paths; compute simple header csum
        uint32_t sum = 0;
        for (int w = 0; w < 10; ++w) {
            sum += (ip[w * 2] << 8) | ip[w * 2 + 1];
        }
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        const uint16_t csum = static_cast<uint16_t>(~sum);
        ip[10] = static_cast<uint8_t>(csum >> 8);
        ip[11] = static_cast<uint8_t>(csum & 0xFF);
        std::memcpy(ip + 20, req.payload.data(), req.payload.size());
        req.msg = NetMsg::IpRawTx;
        req.payload = std::move(frame);
        worker_.post(std::move(req));
        break;
    }
    default:
        break;
    }
}

void W5100Chip::handle_command(size_t i, uint8_t cmd) {
    auto &socket = sockets_[i];
    switch (cmd) {
    case W5100_SN_CR_OPEN:
        open_socket(i);
        break;
    case W5100_SN_CR_LISTEN: {
        NetRequest req;
        req.msg = NetMsg::Listen;
        req.sock = static_cast<uint8_t>(i);
        req.local_port = read_be16(&memory_[socket.registerAddress + W5100_SN_PORT0]);
        worker_.post(std::move(req));
        socket.status = W5100_SN_SR_SOCK_LISTEN;
        break;
    }
    case W5100_SN_CR_CONNECT: {
        NetRequest req;
        req.msg = NetMsg::Connect;
        req.sock = static_cast<uint8_t>(i);
        req.dest_ip = *reinterpret_cast<uint32_t *>(&memory_[socket.registerAddress + W5100_SN_DIPR0]);
        req.dest_port = read_be16(&memory_[socket.registerAddress + W5100_SN_DPORT0]);
        worker_.post(std::move(req));
        socket.status = W5100_SN_SR_SOCK_SYNSENT;
        break;
    }
    case W5100_SN_CR_CLOSE:
    case W5100_SN_CR_DISCON: {
        NetRequest req;
        req.msg = NetMsg::Close;
        req.sock = static_cast<uint8_t>(i);
        worker_.post(std::move(req));
        socket.status = W5100_SN_SR_CLOSED;
        set_header_size(i);
        break;
    }
    case W5100_SN_CR_SEND:
        send_data(i);
        break;
    case W5100_SN_CR_RECV:
        update_rsr(i);
        break;
    default:
        break;
    }
}

uint8_t W5100Chip::read_socket_reg(uint16_t address) {
    const uint16_t i = (address >> 8) - 0x04;
    const uint16_t loc = address & 0xFF;
    switch (loc) {
    case W5100_SN_SR:
        return sockets_[i].status;
    case W5100_SN_TX_FSR0:
        return tx_free_reg(i, 8);
    case W5100_SN_TX_FSR1:
        return tx_free_reg(i, 0);
    case W5100_SN_RX_RSR0:
        drain_events();
        return rx_size_reg(i, 8);
    case W5100_SN_RX_RSR1:
        drain_events();
        return rx_size_reg(i, 0);
    default:
        return memory_[address];
    }
}

void W5100Chip::write_socket_reg(uint16_t address, uint8_t value) {
    const uint16_t i = (address >> 8) - 0x04;
    const uint16_t loc = address & 0xFF;
    switch (loc) {
    case W5100_SN_MR:
        memory_[address] = value;
        break;
    case W5100_SN_CR:
        handle_command(i, value);
        memory_[address] = 0; // CR clears after command
        break;
    default:
        memory_[address] = value;
        break;
    }
}

uint8_t W5100Chip::read_at(uint16_t address) {
    if (address == W5100_MR) {
        return mode_;
    }
    if (address >= W5100_GAR0 && address <= W5100_UPORT1) {
        return memory_[address];
    }
    if (address >= W5100_S0_BASE && address <= W5100_S3_MAX) {
        return read_socket_reg(address);
    }
    if (address >= W5100_TX_BASE && address <= W5100_MEM_MAX) {
        return memory_[address];
    }
    return memory_[address & W5100_MEM_MAX];
}

void W5100Chip::write_at(uint16_t address, uint8_t value) {
    if (address == W5100_MR) {
        if (value & W5100_MR_RST) {
            reset(false);
        } else {
            mode_ = value;
        }
        return;
    }
    if (address == W5100_RMSR) {
        set_rx_sizes(value);
        return;
    }
    if (address == W5100_TMSR) {
        set_tx_sizes(value);
        return;
    }
    if (address >= W5100_MR && address <= W5100_UPORT1) {
        memory_[address] = value;
        return;
    }
    if (address >= W5100_S0_BASE && address <= W5100_S3_MAX) {
        write_socket_reg(address, value);
        return;
    }
    if (address >= W5100_TX_BASE && address <= W5100_MEM_MAX) {
        memory_[address] = value;
    }
}

void W5100Chip::auto_increment() {
    if (mode_ & W5100_MR_AI) {
        ++data_addr_;
        if (data_addr_ == W5100_RX_BASE || data_addr_ == W5100_MEM_SIZE) {
            data_addr_ = static_cast<uint16_t>(data_addr_ - 0x2000);
        }
    }
}

uint8_t W5100Chip::read_value() {
    const uint8_t v = read_at(data_addr_);
    auto_increment();
    return v;
}

void W5100Chip::write_value(uint8_t value) {
    write_at(data_addr_, value);
    auto_increment();
}

uint8_t W5100Chip::io_read(uint8_t loc) {
    drain_events();
    switch (loc & U2_C0X_MASK) {
    case U2_C0X_MODE_REGISTER:
        return mode_;
    case U2_C0X_ADDRESS_HIGH:
        return get_ibyte(data_addr_, 8);
    case U2_C0X_ADDRESS_LOW:
        return get_ibyte(data_addr_, 0);
    case U2_C0X_DATA_PORT:
        return read_value();
    default:
        return 0;
    }
}

void W5100Chip::io_write(uint8_t loc, uint8_t value) {
    drain_events();
    switch (loc & U2_C0X_MASK) {
    case U2_C0X_MODE_REGISTER:
        if (value & W5100_MR_RST) {
            reset(false);
        } else {
            mode_ = value;
        }
        break;
    case U2_C0X_ADDRESS_HIGH:
        data_addr_ = static_cast<uint16_t>((value << 8) | (data_addr_ & 0x00FF));
        break;
    case U2_C0X_ADDRESS_LOW:
        data_addr_ = static_cast<uint16_t>((value << 0) | (data_addr_ & 0xFF00));
        break;
    case U2_C0X_DATA_PORT:
        write_value(value);
        break;
    default:
        break;
    }
}

}  // namespace u2
