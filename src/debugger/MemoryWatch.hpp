#pragma once

#include <cstdint>
#include <vector>

struct watch_entry_t {
    uint32_t start;
    uint32_t end;

    watch_entry_t(uint32_t start, uint32_t end) {
        this->start = start;
        this->end = end;
    }
};

class MemoryWatch {
    std::vector<watch_entry_t> watches;

    public:
        MemoryWatch();
        void add(uint32_t start, uint32_t end);
        void remove(uint32_t start);
        void clear();
        int size() const { return watches.size(); }

        // this is a lot for an iterator, but it's all here..
        using iterator = std::vector<watch_entry_t>::iterator;
        using const_iterator = std::vector<watch_entry_t>::const_iterator;

        iterator begin() { return watches.begin(); }
        iterator end() { return watches.end(); }
        const_iterator begin() const { return watches.begin(); }
        const_iterator end() const { return watches.end(); }
        const_iterator cbegin() const { return watches.cbegin(); }
        const_iterator cend() const { return watches.cend(); }
};
