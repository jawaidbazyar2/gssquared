#include "debugger/MemoryWatch.hpp"

MemoryWatch::MemoryWatch() {
    // Initialize empty watch list
}

void MemoryWatch::add(uint32_t start, uint32_t end) {
    // Create new watch entry
    watch_entry_t entry = {start, end};
    
    // Add to vector
    watches.push_back(entry);
}

void MemoryWatch::remove(uint32_t start) {
    // Remove any watches whose first address matches start
    for (auto it = watches.begin(); it != watches.end(); ++it) {
        if (it->start == start) {
            watches.erase(it);
            break;
        }
    }
}

void MemoryWatch::clear() {
    // Clear all watches
    watches.clear();
}
