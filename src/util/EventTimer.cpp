#include "EventTimer.hpp"
#include <algorithm>
#include <limits>
#include <iostream>
#include "debug.hpp"
//#include "cpu.hpp"

// Constructor implementation
/* EventTimer::EventTimer(cpu_state *cpu) : cpu(cpu) {
} */

// Destructor implementation 
EventTimer::~EventTimer() = default;

inline void EventTimer::updateNextEventCycle() {
    // Update next_event_cycle to the earliest event's trigger time
    next_event_cycle = events.empty() ? std::numeric_limits<uint64_t>::max() : events.front().triggerCycles;
}

// Add a new event to the queue
void EventTimer::scheduleEvent(uint64_t triggerCycles, void (*callback)(uint64_t, void*), uint64_t instanceID, void* userData) {
    if (DEBUG(DEBUG_EVENT_TIMER)) std::cout << "scheduleEvent: " << triggerCycles << " InstanceID: " << instanceID << std::endl;
    if (clock && triggerCycles < clock->get_cycles()) {
        std::cout << "scheduleEvent: Event in the past, skipping" << std::endl;
        return;
    }
    // Check if an event with the same instanceID already exists
    auto existingEvent = std::find_if(events.begin(), events.end(), 
        [instanceID](const Event& event) { return event.instanceID == instanceID; });
    
    if (existingEvent != events.end()) {
        // Replace the existing event with the new one
        existingEvent->triggerCycles = triggerCycles;
        existingEvent->triggerCallback = callback;
        existingEvent->userData = userData;
        
        // Rebuild the heap after modifying an element
        std::make_heap(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.triggerCycles > b.triggerCycles;
        });
    } else {
        // Add a new event to the queue
        Event newEvent{triggerCycles, callback, instanceID, userData};
        events.push_back(newEvent);
        std::push_heap(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.triggerCycles > b.triggerCycles; // Reverse for priority queue (earliest first)
        });
    }
    updateNextEventCycle();
}

// Process all events that should trigger by the given cycle count
void EventTimer::processEvents(uint64_t currentCycles) {
    /* std::cout << "processEvents: " << currentCycles << "Front: " << events.front().triggerCycles << std::endl;
    // Print how many events are on the queue
    if (!events.empty()) {
        std::cout << "Events on queue: " << events.size() << std::endl;
    } */
    while (!events.empty() && events.front().triggerCycles <= currentCycles) {
        Event event = events.front();
        std::pop_heap(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.triggerCycles > b.triggerCycles;
        });
        events.pop_back();
        if (DEBUG(DEBUG_EVENT_TIMER)) std::cout << "Processing event: " << event.triggerCycles << " InstanceID: " << event.instanceID << std::endl;
        // Call the callback function
        if (event.triggerCallback) {
            event.triggerCallback(event.instanceID, event.userData);
        }
    }
    updateNextEventCycle();
}

// Cancel all events for a specific instance
void EventTimer::cancelEvents(uint64_t instanceID) {
    auto newEnd = std::remove_if(events.begin(), events.end(),
        [instanceID](const Event& event) { return event.instanceID == instanceID; });
    
    if (newEnd != events.end()) {
        events.erase(newEnd, events.end());
        // Rebuild the heap after removing elements
        std::make_heap(events.begin(), events.end(), [](const Event& a, const Event& b) {
            return a.triggerCycles > b.triggerCycles;
        });
    }
    updateNextEventCycle();
}

// Check if there are any pending events
bool EventTimer::hasPendingEvents() const {
    return !events.empty();
}

// Get the cycle count of the next event
uint64_t EventTimer::getNextEventCycle() const {
    return next_event_cycle;
}
