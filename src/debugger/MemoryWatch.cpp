#include "debugger/MemoryWatch.hpp"

uint32_t MemoryWatch::add(uint32_t start, uint32_t end) {
    watch_entry_t entry{};
    entry.id = next_id_++;
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    entry.start = start;
    entry.end = end;
    watches.push_back(entry);
    return entry.id;
}

bool MemoryWatch::remove(uint32_t id) {
    for (auto it = watches.begin(); it != watches.end(); ++it) {
        if (it->id == id) {
            watches.erase(it);
            return true;
        }
    }
    return false;
}

void MemoryWatch::clear() {
    watches.clear();
}
