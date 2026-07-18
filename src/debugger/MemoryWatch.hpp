#pragma once

#include <cstdint>
#include <vector>

struct watch_entry_t {
    uint32_t id = 0;
    uint32_t start = 0;
    uint32_t end = 0;
};

class MemoryWatch {
    std::vector<watch_entry_t> watches;
    uint32_t next_id_ = 1;

public:
    MemoryWatch() = default;

    /** Returns new id, or 0 on failure. */
    uint32_t add(uint32_t start, uint32_t end);
    bool remove(uint32_t id);
    void clear();
    int size() const { return static_cast<int>(watches.size()); }

    using iterator = std::vector<watch_entry_t>::iterator;
    using const_iterator = std::vector<watch_entry_t>::const_iterator;

    iterator begin() { return watches.begin(); }
    iterator end() { return watches.end(); }
    const_iterator begin() const { return watches.begin(); }
    const_iterator end() const { return watches.end(); }
    const_iterator cbegin() const { return watches.cbegin(); }
    const_iterator cend() const { return watches.cend(); }
};
