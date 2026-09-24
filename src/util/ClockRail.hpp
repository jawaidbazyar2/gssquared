#pragma once

#include "util/EventTimer.hpp"
#include <cstdint>

class NClock;
class NClockII;
class NClockIIgs;

/* One counter plus the event heap processed against it.
 * Empty subclasses give distinct types without a vtable or a second method body.
 * now_ is the first field so now() is one load from the rail address. */
class ClockRail {
    uint64_t now_ = 0;
    EventTimer events_;

    void add(uint64_t n) { now_ += n; }
    void tick() { ++now_; }

    friend class NClock;
    friend class NClockII;
    friend class NClockIIgs;

public:
    uint64_t now() const { return now_; }

    EventTimer &events() { return events_; }
    const EventTimer &events() const { return events_; }

    bool due() const { return now_ >= events_.getNextEventCycle(); }
    uint64_t next_event() const { return events_.getNextEventCycle(); }

    void process_due() {
        if (due())
            events_.processEvents(now_);
    }

    void schedule(uint64_t when, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        events_.scheduleEvent(when, cb, instanceID, userData);
    }

    void schedule_after(uint64_t delay, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        events_.scheduleEvent(now_ + delay, cb, instanceID, userData);
    }

    void cancel(uint64_t instanceID) {
        events_.cancelEvents(instanceID);
    }
};

class CpuRail : public ClockRail {};
class VidRail : public ClockRail {};
class C14mRail : public ClockRail {};
