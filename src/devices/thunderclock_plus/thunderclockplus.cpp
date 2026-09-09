/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 */

#include "thunderclockplus.hpp"

#include <cstdint>
#include <cstdio>
#include <ctime>

#include "Device_ID.hpp"
#include "debug.hpp"
#include "device_irq_id.hpp"
#include "util/DebugFormatter.hpp"
#include "util/DebugHandlerIDs.hpp"
#include "util/printf_helper.hpp"

namespace {

constexpr uint8_t TCP_DI = 0x01;
constexpr uint8_t TCP_CLK = 0x02;
constexpr uint8_t TCP_STB = 0x04;
constexpr uint8_t TCP_IRQEN = 0x40;
constexpr uint8_t TCP_TP_OUT = 0x40;
constexpr uint8_t TCP_IRQ_STATUS = 0x20;
constexpr uint8_t TCP_DATA_OUT = 0x80;

constexpr uint8_t CMD_HOLD = 0;
constexpr uint8_t CMD_SHIFT = 1;
constexpr uint8_t CMD_TIME_SET = 2;
constexpr uint8_t CMD_TIME_READ = 3;
constexpr uint8_t CMD_TP_64 = 4;
constexpr uint8_t CMD_TP_256 = 5;
constexpr uint8_t CMD_TP_2048 = 6;
constexpr uint8_t CMD_TP_4096 = 7;

constexpr uint64_t TCP_40BIT = 0xFFFFFFFFFFULL;
constexpr uint64_t TCP_TIMER_1HZ = 0x19900000ull;
constexpr uint64_t TCP_TIMER_TP = 0x19910000ull;

constexpr uint8_t kDaysInMonth[13] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

uint64_t timer_id_1hz(SlotType_t slot) {
    return TCP_TIMER_1HZ | static_cast<uint64_t>(slot);
}

uint64_t timer_id_tp(SlotType_t slot) {
    return TCP_TIMER_TP | static_cast<uint64_t>(slot);
}

uint32_t tp_hz_for_cmd(uint8_t cmd) {
    switch (cmd) {
        case CMD_TP_64:
            return 64;
        case CMD_TP_256:
            return 256;
        case CMD_TP_2048:
            return 2048;
        case CMD_TP_4096:
            return 4096;
        default:
            return 0;
    }
}

uint64_t snapshot_host_time() {
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    if (tm == nullptr) {
        return 0;
    }

    const uint8_t nibbles[10] = {
        static_cast<uint8_t>(tm->tm_mon + 1),
        static_cast<uint8_t>(tm->tm_wday),
        static_cast<uint8_t>(tm->tm_mday / 10),
        static_cast<uint8_t>(tm->tm_mday % 10),
        static_cast<uint8_t>(tm->tm_hour / 10),
        static_cast<uint8_t>(tm->tm_hour % 10),
        static_cast<uint8_t>(tm->tm_min / 10),
        static_cast<uint8_t>(tm->tm_min % 10),
        static_cast<uint8_t>(tm->tm_sec / 10),
        static_cast<uint8_t>(tm->tm_sec % 10),
    };

    uint64_t result = 0;
    for (int i = 0; i < 10; i++) {
        result = (result << 4) | (nibbles[i] & 0x0F);
    }
    return result;
}

void increment_time_counter(thunderclock_state *st) {
    uint64_t t = st->time_counter;
    uint8_t sec_ones = static_cast<uint8_t>(t & 0xF);
    uint8_t sec_tens = static_cast<uint8_t>((t >> 4) & 0xF);
    uint8_t min_ones = static_cast<uint8_t>((t >> 8) & 0xF);
    uint8_t min_tens = static_cast<uint8_t>((t >> 12) & 0xF);
    uint8_t hour_ones = static_cast<uint8_t>((t >> 16) & 0xF);
    uint8_t hour_tens = static_cast<uint8_t>((t >> 20) & 0xF);
    uint8_t date_ones = static_cast<uint8_t>((t >> 24) & 0xF);
    uint8_t date_tens = static_cast<uint8_t>((t >> 28) & 0xF);
    uint8_t dow = static_cast<uint8_t>((t >> 32) & 0xF);
    uint8_t month = static_cast<uint8_t>((t >> 36) & 0xF);

    sec_ones++;
    if (sec_ones > 9) {
        sec_ones = 0;
        sec_tens++;
        if (sec_tens > 5) {
            sec_tens = 0;
            min_ones++;
            if (min_ones > 9) {
                min_ones = 0;
                min_tens++;
                if (min_tens > 5) {
                    min_tens = 0;
                    unsigned hour = static_cast<unsigned>(hour_tens) * 10u + hour_ones;
                    hour++;
                    if (hour >= 24) {
                        hour = 0;
                        unsigned date = static_cast<unsigned>(date_tens) * 10u + date_ones;
                        date++;
                        uint8_t dim = (month >= 1 && month <= 12) ? kDaysInMonth[month] : 31;
                        if (date > dim) {
                            date = 1;
                            month++;
                            if (month > 12) {
                                month = 1;
                            }
                        }
                        date_tens = static_cast<uint8_t>(date / 10);
                        date_ones = static_cast<uint8_t>(date % 10);
                        dow = static_cast<uint8_t>((dow + 1) % 7);
                    }
                    hour_tens = static_cast<uint8_t>(hour / 10);
                    hour_ones = static_cast<uint8_t>(hour % 10);
                }
            }
        }
    }

    st->time_counter =
        (static_cast<uint64_t>(sec_ones) & 0xF) |
        ((static_cast<uint64_t>(sec_tens) & 0xF) << 4) |
        ((static_cast<uint64_t>(min_ones) & 0xF) << 8) |
        ((static_cast<uint64_t>(min_tens) & 0xF) << 12) |
        ((static_cast<uint64_t>(hour_ones) & 0xF) << 16) |
        ((static_cast<uint64_t>(hour_tens) & 0xF) << 20) |
        ((static_cast<uint64_t>(date_ones) & 0xF) << 24) |
        ((static_cast<uint64_t>(date_tens) & 0xF) << 28) |
        ((static_cast<uint64_t>(dow) & 0xF) << 32) |
        ((static_cast<uint64_t>(month) & 0xF) << 36);
}

void update_irq(thunderclock_state *st) {
    if (!st->irq_control) {
        return;
    }
    st->irq_control->set_irq(static_cast<device_irq_id>(st->_slot),
                             st->irq_status && st->irqen);
}

void cancel_tp(thunderclock_state *st) {
    st->tp_hz = 0;
    if (st->event_timer) {
        st->event_timer->cancelEvents(timer_id_tp(st->_slot));
    }
}

void cancel_1hz(thunderclock_state *st) {
    if (st->event_timer) {
        st->event_timer->cancelEvents(timer_id_1hz(st->_slot));
    }
}

void schedule_tp(thunderclock_state *st);
void schedule_1hz(thunderclock_state *st);
void start_tp(thunderclock_state *st, uint32_t hz);

void thunderclock_tp_tick(uint64_t /*instanceID*/, void *user) {
    auto *st = static_cast<thunderclock_state *>(user);
    if (st->irqen) {
        st->irq_status = true;
        update_irq(st);
    }
    if (st->tp_hz != 0) {
        schedule_tp(st);
    }
}

void thunderclock_1hz_tick(uint64_t /*instanceID*/, void *user) {
    auto *st = static_cast<thunderclock_state *>(user);
    increment_time_counter(st);
    schedule_1hz(st);
}

void schedule_tp(thunderclock_state *st) {
    if (!st->event_timer || !st->clock || st->tp_hz == 0) {
        return;
    }
    uint64_t period = st->clock->get_c14m_per_second() / st->tp_hz;
    if (period == 0) {
        period = 1;
    }
    st->event_timer->scheduleEvent(st->clock->get_c14m() + period, thunderclock_tp_tick,
                                   timer_id_tp(st->_slot), st);
}

void schedule_1hz(thunderclock_state *st) {
    if (!st->event_timer || !st->clock) {
        return;
    }
    st->event_timer->scheduleEvent(st->clock->get_c14m() + st->clock->get_c14m_per_second(),
                                   thunderclock_1hz_tick, timer_id_1hz(st->_slot), st);
}

void start_tp(thunderclock_state *st, uint32_t hz) {
    if (st->event_timer) {
        st->event_timer->cancelEvents(timer_id_tp(st->_slot));
    }
    st->tp_hz = hz;
    schedule_tp(st);
}

void do_shift(thunderclock_state *st, uint8_t value) {
    const uint64_t di = (value & TCP_DI) ? 1ull : 0ull;
    st->shift_reg = ((di << 39) | (st->shift_reg >> 1)) & TCP_40BIT;
}

void execute_command(thunderclock_state *st) {
    switch (st->latched_cmd) {
        case CMD_HOLD:
        case CMD_SHIFT:
            break;
        case CMD_TIME_SET:
            st->time_counter = st->shift_reg & TCP_40BIT;
            st->time_set_active = true;
            start_tp(st, 64);
            schedule_1hz(st);
            if (DEBUG(DEBUG_THUNDERCLOCK)) {
                printf("TCP TIME SET %010llX\n", u64_t(st->time_counter));
            }
            break;
        case CMD_TIME_READ:
            if (!st->time_set_active) {
                st->time_counter = snapshot_host_time();
            }
            st->shift_reg = st->time_counter & TCP_40BIT;
            if (DEBUG(DEBUG_THUNDERCLOCK)) {
                printf("TCP TIME READ %010llX\n", u64_t(st->shift_reg));
            }
            break;
        case CMD_TP_64:
        case CMD_TP_256:
        case CMD_TP_2048:
        case CMD_TP_4096:
            start_tp(st, tp_hz_for_cmd(st->latched_cmd));
            if (DEBUG(DEBUG_THUNDERCLOCK)) {
                printf("TCP TP %u Hz\n", st->tp_hz);
            }
            break;
        default:
            break;
    }
}

uint8_t data_out_bit(thunderclock_state *st) {
    if (st->latched_cmd == CMD_HOLD && st->clock) {
        uint64_t half = st->clock->get_c14m_per_second() / 2;
        if (half == 0) {
            half = 1;
        }
        return static_cast<uint8_t>((st->clock->get_c14m() / half) & 1u);
    }
    return static_cast<uint8_t>(st->shift_reg & 1u);
}

uint8_t tp_out_bit(thunderclock_state *st) {
    if (!st->clock || st->tp_hz == 0) {
        return 0;
    }
    uint64_t half = st->clock->get_c14m_per_second() / (static_cast<uint64_t>(st->tp_hz) * 2);
    if (half == 0) {
        half = 1;
    }
    return static_cast<uint8_t>((st->clock->get_c14m() / half) & 1u);
}

void thunderclock_reset(thunderclock_state *st, bool cold_start) {
    if (cold_start) {
        st->time_counter = snapshot_host_time();
        st->shift_reg = 0;
        st->latched_cmd = CMD_HOLD;
        st->time_set_active = false;
        cancel_1hz(st);
        start_tp(st, 64);
    }
    st->last_write = 0;
    st->irqen = false;
    st->irq_status = false;
    update_irq(st);
}

uint8_t thunderclock_read_register(void *context, uint32_t address) {
    auto *st = static_cast<thunderclock_state *>(context);
    uint8_t value = 0;
    if (data_out_bit(st)) {
        value |= TCP_DATA_OUT;
    }
    if (tp_out_bit(st)) {
        value |= TCP_TP_OUT;
    }
    if (st->irq_status) {
        value |= TCP_IRQ_STATUS;
    }
    if (DEBUG(DEBUG_THUNDERCLOCK)) {
        printf("TCP read $%04X => $%02X\n", address, value);
    }
    st->irq_status = false;
    update_irq(st);
    return value;
}

void thunderclock_write_register(void *context, uint32_t address, uint8_t value) {
    auto *st = static_cast<thunderclock_state *>(context);
    const uint8_t prev = st->last_write;
    const bool stb_rise = ((prev & TCP_STB) == 0) && ((value & TCP_STB) != 0);
    const bool clk_rise = ((prev & TCP_CLK) == 0) && ((value & TCP_CLK) != 0);

    st->irqen = (value & TCP_IRQEN) != 0;
    update_irq(st);

    if (stb_rise) {
        st->latched_cmd = static_cast<uint8_t>((value >> 3) & 7);
        execute_command(st);
    }
    if (clk_rise && (st->latched_cmd == CMD_SHIFT || st->latched_cmd == CMD_TIME_READ)) {
        do_shift(st, value);
    }

    st->last_write = value;
    if (DEBUG(DEBUG_THUNDERCLOCK)) {
        printf("TCP write $%04X <= $%02X stb=%d clk=%d cmd=%u\n", address, value,
               stb_rise ? 1 : 0, clk_rise ? 1 : 0, st->latched_cmd);
    }
}

void map_rom_thunderclock(void *context, SlotType_t /*slot*/) {
    auto *st = static_cast<thunderclock_state *>(context);
    if (!st || !st->rom || !st->mmu) {
        return;
    }
    uint8_t *dp = st->rom->get_data();
    if (!dp) {
        return;
    }
    for (uint8_t page = 0; page < 8; page++) {
        st->mmu->map_c1cf_page_read_only(page + 0xC8, dp + (page * 0x100), "TCP_ROM");
    }
    if (DEBUG(DEBUG_THUNDERCLOCK)) {
        printf("mapped thunderclock $C800-$CFFF\n");
    }
}

DebugFormatter *debug_thunderclock(thunderclock_state *st) {
    DebugFormatter *df = new DebugFormatter();
    df->addLine("ThunderClock Plus slot %d", static_cast<int>(st->_slot));
    df->addLine("  cmd=%u last=%02X shift=%010llX time=%010llX", st->latched_cmd, st->last_write,
                u64_t(st->shift_reg), u64_t(st->time_counter));
    df->addLine("  irqen=%d irq=%d tp=%u Hz timeset=%d", st->irqen ? 1 : 0, st->irq_status ? 1 : 0,
                st->tp_hz, st->time_set_active ? 1 : 0);
    return df;
}

} // namespace

void init_slot_thunderclock(computer_t *computer, SlotType_t slot) {
    auto *st = new thunderclock_state;
    st->id = DEVICE_ID_THUNDER_CLOCK;
    st->_slot = slot;
    st->computer = computer;
    st->mmu = computer->mmu;
    st->irq_control = computer->irq_control;
    st->clock = computer->clock;
    st->event_timer = computer->event_timer;

    ResourceFile *rom = new ResourceFile("roms/cards/tcp/tcp.rom", READ_ONLY);
    if (rom == nullptr) {
        fprintf(stderr, "ThunderClock: failed to open tcp.rom\n");
        delete st;
        return;
    }
    rom->load();
    if (rom->size() < 0x800) {
        fprintf(stderr, "ThunderClock: ROM too small (%ju bytes), need 2048\n",
                static_cast<uintmax_t>(rom->size()));
        delete rom;
        delete st;
        return;
    }
    st->rom = rom;

    uint8_t *rom_data = st->rom->get_data();
    computer->mmu->set_slot_rom(slot, rom_data, "TCP_ROM");
    computer->mmu->set_C8xx_handler(slot, map_rom_thunderclock, st);

    const uint16_t slot_base = static_cast<uint16_t>(0xC080 + (slot * 0x10));
    for (uint16_t off = 0; off < 0x10; ++off) {
        computer->mmu->set_C0XX_read_handler(slot_base + off, {thunderclock_read_register, st});
        computer->mmu->set_C0XX_write_handler(slot_base + off, {thunderclock_write_register, st});
    }

    thunderclock_reset(st, true);

    computer->register_reset_handler([st](bool cold_start) {
        thunderclock_reset(st, cold_start);
        return true;
    });

    computer->register_shutdown_handler([st]() {
        cancel_tp(st);
        cancel_1hz(st);
        delete st->rom;
        st->rom = nullptr;
        delete st;
        return true;
    });

    computer->register_debug_display_handler("thunderclock", DH_THUNDERCLOCK, [st]() {
        return debug_thunderclock(st);
    });

    fprintf(stdout, "ThunderClock Plus init slot %d ($C0%02X)\n", static_cast<int>(slot),
            0x80 + (static_cast<int>(slot) * 0x10));
}
