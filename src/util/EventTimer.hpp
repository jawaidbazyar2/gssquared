#pragma once

#include <vector>
//#include "gs2.hpp"
//#include "cpu.hpp"
#include "NClock.hpp"

class EventTimer {
public:
    struct Event {
        uint64_t triggerCycles;
        void (*triggerCallback)(uint64_t, void*);
        uint64_t instanceID;
        void* userData;
    };
    NClockII *clock;
    uint64_t next_event_cycle = 0;
    EventTimer(NClockII *clock = nullptr) { this->clock = clock; }
    ~EventTimer();

    void scheduleEvent(uint64_t triggerCycles, void (*callback)(uint64_t, void*), uint64_t instanceID, void* userData = nullptr);
    void processEvents(uint64_t currentCycles);
    void cancelEvents(uint64_t instanceID);
    bool hasPendingEvents() const;
    uint64_t getNextEventCycle() const;
    inline bool isEventPassed(uint64_t currentCycles) { return currentCycles >= next_event_cycle; }
    void set_clock(NClockII *clock) { this->clock = clock; }
    
private:
    std::vector<Event> events;
    void updateNextEventCycle();
};
