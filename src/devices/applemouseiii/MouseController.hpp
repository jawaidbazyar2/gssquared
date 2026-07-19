/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Thorsten Brehm
 *
 * Adapted for GSSquared from A2Pico mouse-interface MouseInterfaceCard.c
 * (instance-per-card; Pico/a2pico deps removed).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include "PIA6520.hpp"

/** AppleMouse III STATE_GET v1 blob size (see Docs/DebugProtocol.md). */
constexpr size_t APPLEMOUSEIII_STATE_GET_V1_SIZE = 32;
/** AppleMouse III STATE_SET v1 request blob size (device payload only). */
constexpr size_t APPLEMOUSEIII_STATE_SET_V1_SIZE = 8;

class MouseController {
public:
    using IrqCallback = std::function<void(bool asserted)>;
    using BankCallback = std::function<void(uint8_t bank)>;
    using VblModeCallback = std::function<void(bool enabled)>;

    void init(IrqCallback irq_cb, BankCallback bank_cb, VblModeCallback vbl_cb);
    void reset();

    uint8_t pia_read(uint16_t address);
    void pia_write(uint32_t address, uint8_t data);

    void move_xy(int8_t x, int8_t y);
    void update_button(uint8_t button_nr, bool pressed);
    void on_vbl();
    void run();

    bool pack_state_get(uint8_t *out, size_t out_len, uint8_t slot) const;
    bool apply_state_set(const uint8_t *blob, size_t len);

    const PIA6520 &pia() const { return pia_; }
    uint8_t operating_mode() const { return operating_mode_; }
    uint8_t int_state() const { return int_state_; }
    bool irq_asserted() const { return irq_asserted_; }
    int16_t x() const { return current_x_; }
    int16_t y() const { return current_y_; }
    int16_t clamp_min_x() const { return clamp_min_x_; }
    int16_t clamp_min_y() const { return clamp_min_y_; }
    int16_t clamp_max_x() const { return clamp_max_x_; }
    int16_t clamp_max_y() const { return clamp_max_y_; }
    bool button0() const { return button0_; }
    bool button1() const { return button1_; }
    uint8_t rom_bank() const { return pia_.rom_bank(); }
    bool vbl_irq_enabled() const {
        return (operating_mode_ & MOUSE_MODE_VBL_IRQ) == MOUSE_MODE_VBL_IRQ;
    }

private:
    static constexpr uint8_t PIA_PORTB_RDACK = 0x10;
    static constexpr uint8_t PIA_PORTB_WRREQUEST = 0x20;
    static constexpr uint8_t PIA_PORTB_RDREADY = 0x40;
    static constexpr uint8_t PIA_PORTB_WRACK = 0x80;

    static constexpr uint8_t COMMAND_SETMOUSE = 0x00;
    static constexpr uint8_t COMMAND_READMOUSE = 0x10;
    static constexpr uint8_t COMMAND_SERVEMOUSE = 0x20;
    static constexpr uint8_t COMMAND_CLEARMOUSE = 0x30;
    static constexpr uint8_t COMMAND_POSMOUSE = 0x40;
    static constexpr uint8_t COMMAND_INITMOUSE = 0x50;
    static constexpr uint8_t COMMAND_CLAMPMOUSE = 0x60;
    static constexpr uint8_t COMMAND_HOMEMOUSE = 0x70;
    static constexpr uint8_t COMMAND_TIMEMOUSE = 0x90;
    static constexpr uint8_t COMMAND_A0 = 0xA0;
    static constexpr uint8_t COMMAND_RDMEMMOUSE = 0xF0;

    static constexpr uint8_t STATUS_WAS_BUTTON1 = (1 << 0);
    static constexpr uint8_t STATUS_IRQ_MOVEMENT = (1 << 1);
    static constexpr uint8_t STATUS_IRQ_BUTTON = (1 << 2);
    static constexpr uint8_t STATUS_IRQ_VBL = (1 << 3);
    static constexpr uint8_t STATUS_IS_BUTTON1 = (1 << 4);
    static constexpr uint8_t STATUS_MOVED = (1 << 5);
    static constexpr uint8_t STATUS_WAS_BUTTON0 = (1 << 6);
    static constexpr uint8_t STATUS_IS_BUTTON0 = (1 << 7);

    static constexpr uint8_t MOUSE_MODE_ENABLED = (1 << 0);
    static constexpr uint8_t MOUSE_MODE_MOVED_IRQ = ((1 << 1) | MOUSE_MODE_ENABLED);
    static constexpr uint8_t MOUSE_MODE_BUTTON_IRQ = ((1 << 2) | MOUSE_MODE_ENABLED);
    static constexpr uint8_t MOUSE_MODE_VBL_IRQ = (1 << 3);

    static constexpr uint16_t US_60HZ_CYCLES = 17030;
    static constexpr uint16_t EU_50HZ_CYCLES = 20280;

    void irq_assert();
    void irq_deassert();
    void notify_bank_if_changed();
    void clamp_xy();

    void command_set();
    void command_read();
    void command_serve();
    void command_clear();
    void command_pos();
    void command_home();
    void command_init();
    void command_read_mem();
    void command_clamp();
    void command_time();
    void command();

    void accept_data();
    void process_write(uint8_t port_b);
    void process_read(uint8_t port_b);

    PIA6520 pia_;
    IrqCallback irq_cb_;
    BankCallback bank_cb_;
    VblModeCallback vbl_cb_;

    uint8_t command_ = 0;
    uint8_t read_buffer_[8]{};
    uint8_t write_buffer_[8]{};
    uint8_t read_pos_ = 0;
    uint8_t write_pos_ = 0;
    uint8_t last_port_b_ = 0;
    uint8_t old_int_ = 0;

    uint16_t intervbl_cycles_ = US_60HZ_CYCLES;
    uint8_t operating_mode_ = 0;
    uint8_t int_state_ = 0;
    bool irq_asserted_ = false;
    bool vbl_pending_ = false;
    uint8_t last_rom_bank_ = 0xFF;

    int16_t current_x_ = 0;
    int16_t current_y_ = 0;
    bool button0_ = false;
    bool button1_ = false;

    int16_t last_x_ = 0;
    int16_t last_y_ = 0;
    bool last_button0_ = false;
    bool last_button1_ = false;

    int16_t clamp_min_x_ = 0;
    int16_t clamp_min_y_ = 0;
    int16_t clamp_max_x_ = 1023;
    int16_t clamp_max_y_ = 1023;
};
