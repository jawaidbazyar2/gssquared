#pragma once

#include "net_worker.hpp"
#include "w5100.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace u2 {

struct SocketState {
    uint16_t transmitBase = 0;
    uint16_t transmitSize = 0;
    uint16_t receiveBase = 0;
    uint16_t receiveSize = 0;
    uint16_t registerAddress = 0;
    uint16_t sn_rx_wr = 0;
    uint16_t sn_rx_rsr = 0;
    uint8_t status = W5100_SN_SR_CLOSED;
    uint8_t headerSize = 0;
};

class W5100Chip {
public:
    W5100Chip();
    ~W5100Chip();

    bool start();
    void stop();
    void reset(bool power_cycle);

    uint8_t io_read(uint8_t loc);
    void io_write(uint8_t loc, uint8_t value);

    /** Drain async network events into chip RX / status. Call on I/O. */
    void drain_events();

private:
    void set_tx_sizes(uint8_t value);
    void set_rx_sizes(uint8_t value);
    void reset_rxtx(size_t i);
    void update_rsr(size_t i);
    void set_header_size(size_t i);

    uint16_t tx_data_size(size_t i) const;
    uint8_t tx_free_reg(size_t i, size_t shift) const;
    uint8_t rx_size_reg(size_t i, size_t shift) const;

    uint8_t read_socket_reg(uint16_t address);
    void write_socket_reg(uint16_t address, uint8_t value);
    uint8_t read_at(uint16_t address);
    void write_at(uint16_t address, uint8_t value);
    void auto_increment();
    uint8_t read_value();
    void write_value(uint8_t value);

    void handle_command(size_t i, uint8_t cmd);
    void open_socket(size_t i);
    void send_data(size_t i);

    void write_rx_byte(size_t i, uint8_t v);
    void write_rx_be16(size_t i, uint16_t v);
    void write_rx_data(size_t i, const uint8_t *data, size_t len);
    bool room_for(size_t i, size_t len) const;

    void apply_event(const NetEvent &ev);
    void ingest_macraw(const NetEvent &ev);

    std::vector<uint8_t> memory_;
    std::array<SocketState, U2_NUM_SOCKETS> sockets_{};
    uint8_t mode_ = 0;
    uint16_t data_addr_ = 0;
    NetWorker worker_;
};

}  // namespace u2
