/*
 *   Emulated time for Second Sight's Z8S180 processor and board devices.
 *   SSCard converts Apple clock ticks to this type. The CPU and its serial
 *   ports compare deadlines with it; the Oak controller receives seconds
 *   through as_seconds. A deadline is the emulated instant when a device
 *   must run next. It has no relation to the host computer's wall clock.
 *
 *   An attosecond is 10^-18 seconds. Values store whole seconds and an
 *   attosecond fraction. PHI is the Z180 clock; a half-PHI is half a clock
 *   period. Integer periods are truncated once, so splitting work into
 *   scheduler slices does not add rounding at each instruction. The never
 *   value means no pending event, rather than a large physical delay.
 *
 *   These are software types, owned by the CPU or device containing them.
 *   Copies may be passed between threads; a live device's timers are touched
 *   only by its execution thread. Hardware clock rules are in Zilog
 *   UM005004-0918 pp. 19-30; the representation and rounding here are model
 *   choices.
 */

#pragma once
#include <algorithm>
#include <cstring>
#include <cassert>
#include <cstdint>
#include <functional>
#include <cmath>

// A value containing an emulated instant or duration. Arithmetic returns new
// values; it neither advances a device nor schedules a host-thread callback.
class Z180Time {
public:
    static constexpr int64_t units_per_second = 1'000'000'000'000'000'000LL;
    static const Z180Time zero, never;
    // Integer emulator time, not a chip register; fraction stays in [0, 10^18)
    // so comparison and addition do not need wide arithmetic in the bus hot path.
    constexpr Z180Time(int64_t seconds = 0, int64_t units = 0)
        : whole(seconds + units / units_per_second), fraction(units % units_per_second) {
        if (fraction < 0) { --whole; fraction += units_per_second; }
    }
    // Split whole seconds before multiplying a truncated tick period; this
    // keeps oscillator rounding independent of the caller execution batch size.
    static Z180Time from_ticks(uint64_t ticks, uint32_t rate) {
        return rate ? Z180Time(ticks / rate, (ticks % rate) * (units_per_second / rate)) : never;
    }
    // Convert changed binary64 deadlines exactly; this is not a per-cycle path.
    static Z180Time from_seconds(double seconds) {
        assert(seconds >= 0);
        if (seconds >= 1'000'000'000.0) return never;
        uint64_t bits;
        std::memcpy(&bits, &seconds, sizeof(bits));
        // Decode IEEE-754 binary64 into integer mantissa times a power of two.
        // Round any discarded fraction upward so a deadline cannot fire early.
        unsigned const exponent = (bits >> 52) & 0x7ff;
        uint64_t const mantissa = (bits & ((uint64_t(1) << 52) - 1)) | (exponent ? uint64_t(1) << 52 : 0);
        unsigned const shift = exponent ? 1075 - exponent : 1074;
        unsigned __int128 const product = (unsigned __int128)mantissa * units_per_second;
        if (shift >= 128) return Z180Time(0, product != 0);
        return from_raw((product >> shift) + ((product & (((unsigned __int128)1 << shift) - 1)) != 0));
    }
    // Round a deadline forward to a clock edge so an access cannot happen early.
    uint64_t ceil_ticks(uint32_t rate) const {
        uint64_t const period = units_per_second / rate;
        return uint64_t(seconds()) * rate + (uint64_t(attoseconds()) + period - 1) / period;
    }
    // Count only complete periods when converting elapsed time to a clock budget.
    uint64_t floor_ticks(uint64_t rate) const {
        uint64_t const period = units_per_second / rate;
        return uint64_t(seconds()) * rate + uint64_t(attoseconds()) / period;
    }
    // Return the period of rate clocks per second; zero rate yields never.
    static Z180Time from_hz(uint32_t rate) { return from_ticks(1, rate); }
    // Count complete timer intervals; large spans use a wide quotient to avoid overflow.
    uint64_t periods(Z180Time period) const {
        if (!whole && !period.whole) return fraction / period.fraction;
        return uint64_t(raw() / period.raw());
    }
    // Whole signed seconds, truncated toward zero for callers using a signed fraction.
    int64_t seconds() const { return whole + (whole < 0 && fraction != 0); }
    // Signed remainder after seconds(), in units of 10^-18 seconds.
    int64_t attoseconds() const { return whole < 0 && fraction ? fraction - units_per_second : fraction; }
    // Convert integer time for the VGA model with nearest-even binary64 rounding.
    double as_seconds() const {
        // Preserve the binary64 integer-conversion rounding before division.
        // Wasm long double is software binary128, unlike Apple Silicon's double.
        const bool negative = whole < 0;
        uint64_t s = negative ? uint64_t(-seconds()) : uint64_t(whole);
        uint64_t f = negative ? uint64_t(-attoseconds()) : uint64_t(fraction);
        auto product = multiply(s, units_per_second);
        uint64_t low = product.low, high = product.high;
        const uint64_t before = low;
        low += f;
        high += low < before;
        double integer;
        if (!high) integer = double(low);
        else {
            unsigned shift = 64 - unsigned(__builtin_clzll(high)) + 11;
            // The normalized value is below 2^124 for the supported int64 seconds.
            uint64_t significand;
            bool half, sticky;
            if (shift < 64) {
                significand = (high << (64 - shift)) | (low >> shift);
                half = (low >> (shift - 1)) & 1;
                sticky = (low & ((uint64_t(1) << (shift - 1)) - 1)) != 0;
            } else {
                const unsigned n = shift - 64;
                significand = high >> n;
                half = n ? (high >> (n - 1)) & 1 : low >> 63;
                sticky = n ? low || (high & ((uint64_t(1) << (n - 1)) - 1)) : (low << 1) != 0;
            }
            // half is the first discarded bit; sticky means any later bit is set.
            // At an exact halfway value, choose the even retained significand.
            significand += half && (sticky || (significand & 1));
            integer = std::ldexp(double(significand), shift);
        }
        double result = integer / units_per_second;
#ifdef __EMSCRIPTEN__
        // The time interface requires the exact ratio rounded to binary64.
        // Correct the binary64 estimate against exact integer midpoints, without
        // a software floating-point operation on each card memory access.
        if (result != 0 && result < 0x1p52) {
            uint64_t bits;
            std::memcpy(&bits, &result, sizeof(bits));
            const unsigned shift = 1075 - unsigned((bits >> 52) & 0x7ff);
            const uint64_t mantissa = (bits & ((uint64_t(1) << 52) - 1)) | (uint64_t(1) << 52);
            const unsigned scale = shift + 1;
            Wide numerator = scale < 64 ? Wide{(high << scale) | (low >> (64 - scale)), low << scale}
                                       : Wide{low << (scale - 64), 0};
            const Wide upper = multiply(2 * mantissa + 1, units_per_second);
            const bool power_of_two = mantissa == (uint64_t(1) << 52);
            const Wide lower = multiply((power_of_two ? 4 : 2) * mantissa - 1, units_per_second);
            const Wide lower_numerator = power_of_two ? Wide{(numerator.high << 1) | (numerator.low >> 63), numerator.low << 1} : numerator;
            auto less = [](Wide a, Wide b) { return a.high < b.high || (a.high == b.high && a.low < b.low); };
            auto equal = [](Wide a, Wide b) { return a.high == b.high && a.low == b.low; };
            if (less(upper, numerator) || (equal(upper, numerator) && (bits & 1))) ++bits;
            else if (less(lower_numerator, lower) || (equal(lower_numerator, lower) && (bits & 1))) --bits;
            std::memcpy(&result, &bits, sizeof(result));
        }
#endif
        return negative ? -result : result;
    }
    // Test the reserved no-deadline range, beginning at one billion seconds.
    bool is_never() const { return whole >= 1'000'000'000; }
    // Add durations (or a duration to an instant), carrying the fraction; never propagates.
    friend Z180Time operator+(Z180Time a, Z180Time b) {
        if (a.is_never() || b.is_never()) return never;
        int64_t units = a.fraction + b.fraction;
        int64_t seconds = a.whole + b.whole;
        if (units >= units_per_second) { units -= units_per_second; ++seconds; }
        return normalized(seconds, units);
    }
    // Subtract instants/durations and borrow a second when needed; never on the left propagates.
    friend Z180Time operator-(Z180Time a, Z180Time b) {
        if (a.is_never()) return never;
        int64_t units = a.fraction - b.fraction;
        int64_t seconds = a.whole - b.whole;
        if (units < 0) { units += units_per_second; --seconds; }
        return normalized(seconds, units);
    }
    // Scale a duration by n periods; use wide arithmetic only when the fraction can overflow.
    friend Z180Time operator*(Z180Time a, uint64_t n) {
        if (a.is_never()) return never;
        if (!a.whole && (!n || uint64_t(a.fraction) <= uint64_t(INT64_MAX) / n))
            return Z180Time(0, a.fraction * n);
        return from_raw(a.raw() * n);
    }
    // Divide a duration by nonzero n, truncating sub-attosecond remainder.
    friend Z180Time operator/(Z180Time a, unsigned n) {
        if (a.is_never()) return never;
        if (!a.whole) return normalized(0, a.fraction / n);
        return from_raw(a.raw() / n);
    }
    // Compare normalized values, including the never sentinel.
    friend bool operator==(Z180Time a, Z180Time b) { return a.whole == b.whole && a.fraction == b.fraction; }
    // True when the two instants/durations differ.
    friend bool operator!=(Z180Time a, Z180Time b) { return !(a == b); }
    // Order by whole seconds, then fraction; finite times precede never.
    friend bool operator<(Z180Time a, Z180Time b) {
        return a.whole < b.whole || (a.whole == b.whole && a.fraction < b.fraction);
    }
    // True when a follows b in emulated time.
    friend bool operator>(Z180Time a, Z180Time b) { return b < a; }
    // True when a is no later than b.
    friend bool operator<=(Z180Time a, Z180Time b) { return !(b < a); }
    // True when a is no earlier than b.
    friend bool operator>=(Z180Time a, Z180Time b) { return !(a < b); }

private:
    // Temporary unsigned 128-bit value for rounding, local to this call/thread.
    struct Wide { uint64_t high, low; };
    // Form a 128-bit product from 32-bit limbs for exact rounding comparisons
    // without introducing a software floating-point helper into each bus access.
    static Wide multiply(uint64_t a, uint64_t b) {
        constexpr uint64_t mask = 0xffffffff;
        uint64_t low = (a & mask) * (b & mask);
        uint64_t middle = (low >> 32) + (a & mask) * (b >> 32);
        uint64_t high = (middle >> 32) + (a >> 32) * (b >> 32);
        middle = (middle & mask) + (a >> 32) * (b & mask);
        return {high + (middle >> 32), (middle << 32) | (low & mask)};
    }
    int64_t whole, fraction;
    // Construct from an already normalized fraction, avoiding another division.
    static Z180Time normalized(int64_t seconds, int64_t units) {
        Z180Time t;
        t.whole = seconds; t.fraction = units;
        return t;
    }
    // Flatten the signed value to attoseconds for checkpoints and slow-path arithmetic.
    __int128 raw() const { return __int128(whole) * units_per_second + fraction; }
    // Split a signed attosecond count back into the normalized representation.
    static Z180Time from_raw(__int128 value) {
        return Z180Time(value / units_per_second, value % units_per_second);
    }
};
inline const Z180Time Z180Time::zero{};
inline const Z180Time Z180Time::never{1'000'000'000};

// Device-owned deadline record, used only on the CPU execution thread. The
// owner checks deadline and calls its own handler; this object starts no thread.
// period=never makes a one-shot timer. now must be bound before adjust/remaining.
struct Z180Timer {
    Z180Time deadline = Z180Time::never, period = Z180Time::never;
    std::function<Z180Time()> now;
    // Schedule a peripheral relative to its emulated clock, not wall-clock time.
    // delay is relative to now(); repeat is the interval or never for one shot.
    // The unused integer preserves the peripheral call interface.
    void adjust(Z180Time delay, int = 0, Z180Time repeat = Z180Time::never) {
        deadline = now() + delay;
        period = repeat;
    }
    // Return whether this device has scheduled a finite deadline.
    bool enabled() const { return !deadline.is_never(); }
    // Return time until expiry at the owner's clock; negative means already overdue.
    Z180Time remaining() const { return deadline - now(); }
    // Advance from the previous deadline so repeated events do not accumulate
    // caller dispatch latency.
    void expire() { deadline = period.is_never() ? Z180Time::never : deadline + period; }
};
