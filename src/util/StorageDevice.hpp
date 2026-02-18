#pragma once

#include <cstdint>
#include "media.hpp"
#include "drive_status.hpp"

class StorageDevice {
    public:
        virtual ~StorageDevice() = default;
        
        virtual bool mount(uint64_t key, media_descriptor *media) = 0;
        virtual bool unmount(uint64_t key) = 0;
        virtual bool writeback(uint64_t key) = 0;
        virtual drive_status_t status(uint64_t key) = 0;
        //virtual drive_type_t drive_type() const = 0;
};