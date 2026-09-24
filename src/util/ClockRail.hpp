#pragma once

#include "util/EventTimer.hpp"
#include <cassert>
#include <cstdint>

class NClock;
class NClockII;
class NClockIIgs;

struct CpuCycles {
    uint64_t v;
    explicit CpuCycles(uint64_t n) : v(n) {}
};

struct VidCycles {
    uint64_t v;
    explicit VidCycles(uint64_t n) : v(n) {}
};

struct C14mTicks {
    uint64_t v;
    explicit C14mTicks(uint64_t n) : v(n) {}
};

/* One counter plus the event heap processed against it.
 * Empty subclasses give distinct types without a vtable or a second method body.
 * now_ is the first field so now() is one load from the rail address. */
class ClockRail {
protected:
    uint64_t now_ = 0;

private:
    EventTimer events_;

    void add(uint64_t n) { now_ += n; }
    void tick() { ++now_; }

    friend class NClock;
    friend class NClockII;
    friend class NClockIIgs;

protected:
    uint64_t next_event_raw() const { return events_.getNextEventCycle(); }

    void schedule_raw(uint64_t when, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        // when == now_ is legal (next poll). when < now_ is a programmer error.
        // Debug asserts; release still queues so process_due runs it immediately.
        assert(when >= now_);
        events_.scheduleEvent(when, cb, instanceID, userData);
    }

    void schedule_after_raw(uint64_t delay, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        events_.scheduleEvent(now_ + delay, cb, instanceID, userData);
    }

public:
    EventTimer &events() { return events_; }
    const EventTimer &events() const { return events_; }

    bool due() const { return now_ >= events_.getNextEventCycle(); }

    void process_due() {
        if (due())
            events_.processEvents(now_);
    }

    void cancel(uint64_t instanceID) {
        events_.cancelEvents(instanceID);
    }
};

class CpuRail : public ClockRail {
public:
    CpuCycles now() const { return CpuCycles{now_}; }
    CpuCycles next_event() const { return CpuCycles{next_event_raw()}; }

    void schedule(CpuCycles when, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_raw(when.v, cb, instanceID, userData);
    }

    void schedule_after(CpuCycles delay, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_after_raw(delay.v, cb, instanceID, userData);
    }
};

class VidRail : public ClockRail {
public:
    VidCycles now() const { return VidCycles{now_}; }
    VidCycles next_event() const { return VidCycles{next_event_raw()}; }

    void schedule(VidCycles when, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_raw(when.v, cb, instanceID, userData);
    }

    void schedule_after(VidCycles delay, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_after_raw(delay.v, cb, instanceID, userData);
    }
};

class C14mRail : public ClockRail {
public:
    C14mTicks now() const { return C14mTicks{now_}; }
    C14mTicks next_event() const { return C14mTicks{next_event_raw()}; }

    void schedule(C14mTicks when, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_raw(when.v, cb, instanceID, userData);
    }

    void schedule_after(C14mTicks delay, void (*cb)(uint64_t, void*), uint64_t instanceID, void *userData = nullptr) {
        schedule_after_raw(delay.v, cb, instanceID, userData);
    }
};
