/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Thorsten Brehm
 *
 * Adapted for GSSquared (instance-per-card, no Pico deps).
 *
 * PIA6520: Rockwell R6520 emulation for the Apple Mouse Interface Card.
 * PIA IRQ outputs are not connected on the real card and are omitted here.
 */

#include "PIA6520.hpp"

void PIA6520::init() {
    DDRA = DDRB = ORA = ORB = CRA = CRB = IA = IB = 0;
}

void PIA6520::write(uint32_t address, uint8_t data) {
    switch (address & 3) {
        case 0:
            if (CRA & 0x04) {
                ORA = data;
            } else {
                DDRA = data;
            }
            break;
        case 1:
            CRA = data & 0x3f;
            break;
        case 2:
            if (CRB & 0x04) {
                ORB = data;
            } else {
                DDRB = data;
            }
            break;
        case 3:
            CRB = data & 0x3f;
            break;
    }
}

uint8_t PIA6520::read(uint16_t address) const {
    switch (address & 3) {
        case 0:
            return (CRA & 0x04) ? port_a() : DDRA;
        case 1:
            return CRA;
        case 2:
            return (CRB & 0x04) ? port_b() : DDRB;
        default:
            return CRB;
    }
}
