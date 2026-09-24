#pragma once

#include <vector>
#include <cstdint>

class EventTimer {
public:
    struct Event {
        uint64_t triggerCycles;
        void (*triggerCallback)(uint64_t, void*);
        uint64_t instanceID;
        void* userData;
    };
    uint64_t next_event_cycle = 0;
    EventTimer() = default;
    ~EventTimer();

    void scheduleEvent(uint64_t triggerCycles, void (*callback)(uint64_t, void*), uint64_t instanceID, void* userData = nullptr);
    void processEvents(uint64_t currentCycles);
    void cancelEvents(uint64_t instanceID);
    bool hasPendingEvents() const;
    uint64_t getNextEventCycle() const;
    inline bool isEventPassed(uint64_t currentCycles) { return currentCycles >= next_event_cycle; }
    
private:
    std::vector<Event> events;
    void updateNextEventCycle();
};
