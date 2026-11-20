/*
 *   Copyright (c) 2025-2026 Jawaid Bazyar

 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.

 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.

 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdint>
#include <vector>

extern "C" {
#include "ssi263/ssi263.h"
}

// Clean-room Rosenberg glottal pulse from mockingboard-pico mb_speech.c
// (not MAME's GPL SC-01 wave). The ssi263 engine takes the table as data.
static const double SSI263_GLOTTAL[] = {
    0.750000, 0.000632, 0.002476, 0.005384, 0.009119, 0.013378, 0.017818, 0.022078,
    0.025813, 0.028721, 0.030565, 0.031197, 0.028822, 0.022059, 0.011938,
};
static constexpr int SSI263_GLOTTAL_N =
    (int)(sizeof(SSI263_GLOTTAL) / sizeof(SSI263_GLOTTAL[0]));

class Ssi263 {
private:
    ssi263 *chip_ = nullptr;
    ssi263_params params_{};
    // Mockingboard A/R is a latch (AppleWin GH#175 / GH#1197):
    // set at phoneme complete, cleared by writes to R0/R1/R2 or R3 CTL=1.
    // The phoneme itself repeats until R0 changes or CTL=1 — that restart
    // must NOT be tied to the ACK, or inflection (R1) writes chop the
    // phoneme and flood CA1 while music timers are running (Berzap).
    bool ar_asserted_ = false;
    bool prev_engine_req_ = false;

    void poll_ar() {
        if (!chip_) {
            ar_asserted_ = false;
            prev_engine_req_ = false;
            return;
        }
        const bool eng = ssi263_request(chip_) != 0;
        if (eng && !prev_engine_req_) {
            ar_asserted_ = true;
            // Repeat the current phoneme (timer restarts; A/R stays latched
            // until an ACK write). mb-audit T263_6 Part-1 gets the next
            // CA1 edge only after that ACK.
            ssi263_write(chip_, 0, ssi263_reg(chip_, 0));
            prev_engine_req_ = ssi263_request(chip_) != 0;
            return;
        }
        prev_engine_req_ = eng;
    }

    void build(double xck_hz, double out_rate) {
        if (chip_) {
            ssi263_free(chip_);
            chip_ = nullptr;
        }
        ssi263_default_params(&params_);
        params_.xck_hz = (real)xck_hz;
        params_.output_oversample = 1.0f; // FIR cost; Pico uses 1 on the same core
        params_.glottal_n = (real)SSI263_GLOTTAL_N;
        // Host A/R is phoneme-complete, not the engine's early-request
        // lookahead used to queue the next stop burst.
        params_.release_lookahead = 0.0f;
        for (int i = 0; i < SSI263_GLOTTAL_N; i++) {
            params_.glottal_wave[i] = (real)SSI263_GLOTTAL[i];
        }
        chip_ = ssi263_new(&params_, ssi263_default_rom(), (real)out_rate);
        ar_asserted_ = false;
        prev_engine_req_ = false;
    }

public:
    Ssi263(double xck_hz, double out_rate) {
        build(xck_hz, out_rate);
    }

    ~Ssi263() {
        if (chip_) {
            ssi263_free(chip_);
            chip_ = nullptr;
        }
    }

    Ssi263(const Ssi263 &) = delete;
    Ssi263 &operator=(const Ssi263 &) = delete;

    void reset(double xck_hz, double out_rate) {
        build(xck_hz, out_rate);
    }

    void set_xck(double xck_hz) {
        if (chip_) {
            ssi263_set_xck(chip_, (real)xck_hz);
        }
    }

    // SSI-263 register write. CTL (R3) D7 high→low starts the phoneme, matching
    // real hardware / Pico apply_write(): the engine sets mode on that edge but
    // only load_phoneme()s on an R0 write, so retrigger R0.
    void write(int reg, uint8_t val) {
        if (!chip_) {
            return;
        }
        const int prev_ctl = ssi263_reg(chip_, 3);
        ssi263_write(chip_, reg, val);
        if (reg == 3 && (prev_ctl & 0x80) && !(val & 0x80)) {
            ssi263_write(chip_, 0, ssi263_reg(chip_, 0));
        }
        // 6522 CA2 is not wired to the SSI-263. Writes to R0, R1, R2 (and
        // R3 CTL=1) ACK A/R only — they do not restart the phoneme.
        if (reg <= 2 || (reg == 3 && (val & 0x80))) {
            ar_asserted_ = false;
        }
        poll_ar();
    }

    bool request() const {
        return ar_asserted_;
    }

    int reg(int i) const {
        return chip_ ? ssi263_reg(chip_, i) : 0;
    }

    long run(long n, float *out) {
        if (!chip_ || n <= 0) {
            return 0;
        }
        const long got = ssi263_run(chip_, n, out);
        poll_ar();
        return got;
    }

    void skip(double seconds) {
        if (chip_ && seconds > 0.0) {
            ssi263_skip(chip_, (real)seconds);
            poll_ar();
        }
    }
};
