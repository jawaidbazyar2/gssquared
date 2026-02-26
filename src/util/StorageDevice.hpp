#pragma once

#include <cstdint>
#include <functional>
#include "media.hpp"
#include "drive_status.hpp"

struct storage_key_t {
    union {
        uint64_t key;
        struct {
            uint16_t partition;
            uint16_t subunit;
            uint16_t drive;
            uint16_t slot;
        };
    };

    storage_key_t() : key(0) {}
    storage_key_t(uint64_t v) : key(v) {}
    storage_key_t& operator=(uint64_t v) { key = v; return *this; }
    operator uint64_t() const { return key; }
    bool operator==(const storage_key_t& other) const { return key == other.key; }
};

template<>
struct std::hash<storage_key_t> {
    size_t operator()(const storage_key_t& k) const noexcept {
        return std::hash<uint64_t>{}(k.key);
    }
};

class StorageDevice {
    public:
        virtual ~StorageDevice() = default;
        
        virtual bool mount(storage_key_t key, media_descriptor *media) = 0;
        virtual bool unmount(storage_key_t key) = 0;
        virtual bool writeback(storage_key_t key) = 0;
        virtual drive_status_t status(storage_key_t key) = 0;
};