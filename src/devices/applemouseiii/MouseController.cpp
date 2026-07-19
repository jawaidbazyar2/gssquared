/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Thorsten Brehm
 *
 * Adapted for GSSquared from A2Pico mouse-interface MouseInterfaceCard.c
 * (instance-per-card; Pico/a2pico deps removed).
 */

#include "MouseController.hpp"

#include <cstring>

void MouseController::init(IrqCallback irq_cb, BankCallback bank_cb, VblModeCallback vbl_cb) {
    irq_cb_ = std::move(irq_cb);
    bank_cb_ = std::move(bank_cb);
    vbl_cb_ = std::move(vbl_cb);
    reset();
}

void MouseController::irq_assert() {
    if (!irq_asserted_) {
        irq_asserted_ = true;
        if (irq_cb_) {
            irq_cb_(true);
        }
    }
}

void MouseController::irq_deassert() {
    if (irq_asserted_) {
        irq_asserted_ = false;
        if (irq_cb_) {
            irq_cb_(false);
        }
    }
}

void MouseController::notify_bank_if_changed() {
    const uint8_t bank = pia_.rom_bank();
    if (bank != last_rom_bank_) {
        last_rom_bank_ = bank;
        if (bank_cb_) {
            bank_cb_(bank);
        }
    }
}

void MouseController::clamp_xy() {
    if (current_x_ < clamp_min_x_) {
        current_x_ = clamp_min_x_;
    }
    if (current_y_ < clamp_min_y_) {
        current_y_ = clamp_min_y_;
    }
    if (current_x_ > clamp_max_x_) {
        current_x_ = clamp_max_x_;
    }
    if (current_y_ > clamp_max_y_) {
        current_y_ = clamp_max_y_;
    }
}

void MouseController::command_set() {
    operating_mode_ = command_ & 0x0F;
    if (vbl_cb_) {
        vbl_cb_(vbl_irq_enabled());
    }
}

void MouseController::command_read() {
    uint8_t st = int_state_ & STATUS_MOVED;
    if (last_button0_) {
        st |= STATUS_WAS_BUTTON0;
    }
    if (last_button1_) {
        st |= STATUS_WAS_BUTTON1;
    }
    if (button0_) {
        st |= STATUS_IS_BUTTON0;
    }
    if (button1_) {
        st |= STATUS_IS_BUTTON1;
    }

    read_buffer_[4] = static_cast<uint8_t>(current_x_ & 0xff);
    read_buffer_[3] = static_cast<uint8_t>((current_x_ >> 8) & 0xff);
    read_buffer_[2] = static_cast<uint8_t>(current_y_ & 0xff);
    read_buffer_[1] = static_cast<uint8_t>((current_y_ >> 8) & 0xff);
    read_buffer_[0] = st;

    int_state_ = st & ~STATUS_MOVED;
    last_x_ = current_x_;
    last_y_ = current_y_;
    last_button0_ = button0_;
    last_button1_ = button1_;
    read_pos_ = 5;
}

void MouseController::command_serve() {
    read_buffer_[0] = int_state_ & ~(1 << 5);
    read_pos_ = 1;
    int_state_ &= ~(STATUS_IRQ_VBL | STATUS_IRQ_MOVEMENT | STATUS_IRQ_BUTTON);
    irq_deassert();
}

void MouseController::command_clear() {
    current_x_ = current_y_ = 0;
}

void MouseController::command_pos() {
    current_x_ = static_cast<int16_t>(write_buffer_[3] | (write_buffer_[2] << 8));
    current_y_ = static_cast<int16_t>(write_buffer_[1] | (write_buffer_[0] << 8));
    clamp_xy();
    last_x_ = current_x_;
    last_y_ = current_y_;
}

void MouseController::command_home() {
    current_x_ = last_x_ = clamp_min_x_;
    current_y_ = last_y_ = clamp_min_y_;
}

void MouseController::command_init() {
    clamp_max_x_ = clamp_max_y_ = 1023;
    clamp_min_x_ = clamp_min_y_ = 0;
    command_home();
    irq_deassert();
}

void MouseController::command_read_mem() {
    const uint16_t address = static_cast<uint16_t>(write_buffer_[1] | (write_buffer_[0] << 8));
    switch (address) {
        case 0x47:
            read_buffer_[0] = static_cast<uint8_t>(clamp_min_x_ >> 8);
            break;
        case 0x48:
            read_buffer_[0] = static_cast<uint8_t>(clamp_min_y_ >> 8);
            break;
        case 0x49:
            read_buffer_[0] = static_cast<uint8_t>(clamp_min_x_);
            break;
        case 0x4a:
            read_buffer_[0] = static_cast<uint8_t>(clamp_min_y_);
            break;
        case 0x4b:
            read_buffer_[0] = static_cast<uint8_t>(clamp_max_x_ >> 8);
            break;
        case 0x4c:
            read_buffer_[0] = static_cast<uint8_t>(clamp_max_y_ >> 8);
            break;
        case 0x4d:
            read_buffer_[0] = static_cast<uint8_t>(clamp_max_x_);
            break;
        case 0x4e:
            read_buffer_[0] = static_cast<uint8_t>(clamp_max_y_);
            break;
        default:
            read_buffer_[0] = 0x00;
            break;
    }
    read_pos_ = 1;
}

void MouseController::command_clamp() {
    int16_t min_clamp = static_cast<int16_t>(write_buffer_[3] | (write_buffer_[1] << 8));
    int16_t max_clamp = static_cast<int16_t>(write_buffer_[2] | (write_buffer_[0] << 8));

    if (min_clamp > max_clamp) {
        uint32_t t = static_cast<uint32_t>(max_clamp) + static_cast<uint32_t>(min_clamp);
        max_clamp = static_cast<int16_t>(t >> 1);
        min_clamp = 0;
    }

    if (command_ & 0x1) {
        clamp_min_y_ = min_clamp;
        clamp_max_y_ = max_clamp;
    } else {
        clamp_min_x_ = min_clamp;
        clamp_max_x_ = max_clamp;
    }
    clamp_xy();
}

void MouseController::command_time() {
    intervbl_cycles_ = (command_ & 0x1) ? EU_50HZ_CYCLES : US_60HZ_CYCLES;
}

void MouseController::command() {
    switch (command_ & 0xF0) {
        case COMMAND_SETMOUSE:
            command_set();
            break;
        case COMMAND_READMOUSE:
            command_read();
            break;
        case COMMAND_SERVEMOUSE:
            command_serve();
            break;
        case COMMAND_CLEARMOUSE:
            command_clear();
            break;
        case COMMAND_POSMOUSE:
            command_pos();
            break;
        case COMMAND_INITMOUSE:
            command_init();
            break;
        case COMMAND_CLAMPMOUSE:
            command_clamp();
            break;
        case COMMAND_HOMEMOUSE:
            command_home();
            break;
        case COMMAND_TIMEMOUSE:
            command_time();
            break;
        case COMMAND_RDMEMMOUSE:
            command_read_mem();
            break;
        default:
            break;
    }
}

void MouseController::accept_data() {
    if (write_pos_) {
        write_buffer_[--write_pos_] = pia_.port_a();
    } else {
        command_ = pia_.port_a();
        switch (command_ & 0xF0) {
            case COMMAND_POSMOUSE:
                write_pos_ = 4;
                break;
            case COMMAND_CLAMPMOUSE:
                write_pos_ = 4;
                break;
            case COMMAND_A0:
                write_pos_ = 1;
                break;
            case COMMAND_RDMEMMOUSE:
                write_pos_ = 2;
                break;
            case COMMAND_TIMEMOUSE:
                switch (command_ & 0xc) {
                    case 0x4:
                        write_pos_ = 2;
                        break;
                    case 0x8:
                        write_pos_ = 1;
                        break;
                    case 0xc:
                        write_pos_ = 3;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    if (write_pos_ == 0) {
        command();
    }
}

void MouseController::process_write(uint8_t port_b) {
    if (port_b & PIA_PORTB_WRREQUEST) {
        read_pos_ = 0;
        accept_data();
        pia_.input_b(static_cast<uint8_t>((pia_.IB & ~PIA_PORTB_RDREADY) | PIA_PORTB_WRACK));
    } else {
        if (pia_.IB & PIA_PORTB_WRACK) {
            pia_.input_b(static_cast<uint8_t>(pia_.IB & ~PIA_PORTB_WRACK));
        }
    }
}

void MouseController::process_read(uint8_t port_b) {
    if (port_b & PIA_PORTB_RDACK) {
        if (pia_.IB & PIA_PORTB_RDREADY) {
            if (read_pos_ > 0) {
                read_pos_--;
            }
            pia_.input_b(static_cast<uint8_t>(pia_.IB & ~PIA_PORTB_RDREADY));
        }
    } else {
        if ((port_b & (PIA_PORTB_WRACK | PIA_PORTB_WRREQUEST)) == 0) {
            if ((pia_.IB & PIA_PORTB_RDREADY) == 0) {
                pia_.input_a(read_pos_ > 0 ? read_buffer_[read_pos_ - 1] : 0x00);
                pia_.input_b(static_cast<uint8_t>(pia_.IB | PIA_PORTB_RDREADY));
            }
        }
    }
}

void MouseController::move_xy(int8_t x, int8_t y) {
    const int16_t old_x = current_x_;
    const int16_t old_y = current_y_;

    if (x > 0) {
        current_x_ = static_cast<int16_t>(current_x_ + x);
        if ((current_x_ < old_x) || (current_x_ > clamp_max_x_)) {
            current_x_ = clamp_max_x_;
        }
    } else {
        current_x_ = static_cast<int16_t>(current_x_ + x);
        if ((current_x_ > old_x) || (current_x_ < clamp_min_x_)) {
            current_x_ = clamp_min_x_;
        }
    }
    if (y > 0) {
        current_y_ = static_cast<int16_t>(current_y_ + y);
        if ((current_y_ < old_y) || (current_y_ > clamp_max_y_)) {
            current_y_ = clamp_max_y_;
        }
    } else {
        current_y_ = static_cast<int16_t>(current_y_ + y);
        if ((current_y_ > old_y) || (current_y_ < clamp_min_y_)) {
            current_y_ = clamp_min_y_;
        }
    }

    if ((current_x_ != old_x) || (current_y_ != old_y)) {
        int_state_ |= STATUS_MOVED;
        if ((operating_mode_ & MOUSE_MODE_MOVED_IRQ) == MOUSE_MODE_MOVED_IRQ) {
            int_state_ |= STATUS_IRQ_MOVEMENT;
        }
    }
    run();
}

void MouseController::update_button(uint8_t button_nr, bool pressed) {
    if (button_nr == 0) {
        button0_ = pressed;
    } else {
        button1_ = pressed;
    }
    if ((operating_mode_ & MOUSE_MODE_BUTTON_IRQ) == MOUSE_MODE_BUTTON_IRQ) {
        int_state_ |= STATUS_IRQ_BUTTON;
    }
    run();
}

void MouseController::on_vbl() {
    if (vbl_irq_enabled()) {
        vbl_pending_ = true;
    }
    run();
}

void MouseController::run() {
    const uint8_t port_b = pia_.port_b();

    if ((port_b ^ last_port_b_) & PIA_PORTB_WRREQUEST) {
        process_write(port_b);
    }
    process_read(port_b);
    last_port_b_ = port_b;

    if (vbl_pending_) {
        vbl_pending_ = false;
        int_state_ |= STATUS_IRQ_VBL;
    }

    const uint8_t irq_bits = STATUS_IRQ_VBL | STATUS_IRQ_MOVEMENT | STATUS_IRQ_BUTTON;
    if (int_state_ & irq_bits) {
        if ((old_int_ & irq_bits) == 0) {
            irq_assert();
        }
    }
    old_int_ = int_state_;
}

void MouseController::reset() {
    irq_deassert();
    pia_.init();
    command_ = 0;
    std::memset(read_buffer_, 0, sizeof(read_buffer_));
    std::memset(write_buffer_, 0, sizeof(write_buffer_));
    read_pos_ = write_pos_ = 0;
    last_port_b_ = 0;
    old_int_ = 0;
    intervbl_cycles_ = US_60HZ_CYCLES;
    operating_mode_ = 0;
    int_state_ = 0;
    vbl_pending_ = false;
    current_x_ = current_y_ = 0;
    button0_ = button1_ = false;
    last_x_ = last_y_ = 0;
    last_button0_ = last_button1_ = false;
    clamp_min_x_ = clamp_min_y_ = 0;
    clamp_max_x_ = clamp_max_y_ = 1023;
    last_rom_bank_ = 0xFF;
    notify_bank_if_changed();
    if (vbl_cb_) {
        vbl_cb_(false);
    }
}

uint8_t MouseController::pia_read(uint16_t address) {
    run();
    const uint8_t val = pia_.read(address);
    run();
    return val;
}

void MouseController::pia_write(uint32_t address, uint8_t data) {
    pia_.write(address, data);
    notify_bank_if_changed();
    run();
}

bool MouseController::pack_state_get(uint8_t *out, size_t out_len, uint8_t slot) const {
    if (!out || out_len < APPLEMOUSEIII_STATE_GET_V1_SIZE) {
        return false;
    }
    std::memset(out, 0, APPLEMOUSEIII_STATE_GET_V1_SIZE);
    uint32_t version = 1;
    std::memcpy(out + 0, &version, 4);
    out[4] = slot;
    out[5] = pia_.rom_bank();
    out[6] = operating_mode_;
    out[7] = int_state_;
    out[8] = irq_asserted_ ? 1 : 0;
    out[9] = button0_ ? 1 : 0;
    out[10] = button1_ ? 1 : 0;
    out[11] = 0;
    std::memcpy(out + 12, &current_x_, 2);
    std::memcpy(out + 14, &current_y_, 2);
    std::memcpy(out + 16, &clamp_min_x_, 2);
    std::memcpy(out + 18, &clamp_min_y_, 2);
    std::memcpy(out + 20, &clamp_max_x_, 2);
    std::memcpy(out + 22, &clamp_max_y_, 2);
    out[24] = pia_.ORA;
    out[25] = pia_.ORB;
    out[26] = pia_.DDRA;
    out[27] = pia_.DDRB;
    out[28] = pia_.CRA;
    out[29] = pia_.CRB;
    out[30] = pia_.IA;
    out[31] = pia_.IB;
    return true;
}

bool MouseController::apply_state_set(const uint8_t *blob, size_t len) {
    if (!blob || len < APPLEMOUSEIII_STATE_SET_V1_SIZE) {
        return false;
    }
    uint32_t version = 0;
    std::memcpy(&version, blob, 4);
    if (version != 1) {
        return false;
    }
    const uint8_t flags = blob[4];
    const int8_t dx = static_cast<int8_t>(blob[5]);
    const int8_t dy = static_cast<int8_t>(blob[6]);
    const uint8_t buttons = blob[7];
    if (flags & 0x01) {
        move_xy(dx, dy);
    }
    if (flags & 0x02) {
        update_button(0, (buttons & 0x01) != 0);
        update_button(1, (buttons & 0x02) != 0);
    }
    return true;
}
