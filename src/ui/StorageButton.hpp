#pragma once

#include "Button.hpp"
#include "util/mount.hpp"

// Or should we just add an Interface?

class StorageButton : public Button_t {
    protected:
        uint64_t key;
        drive_status_t status = { .is_mounted = false, .motor_on = false, .position = 0, .filename = "" };
    public:
    // use same constructors as Button_t.
        using Button_t::Button_t;
    
        // Disk state setters and getters
        /* virtual void set_disk_slot(int slot);
        virtual int get_disk_slot() const;
        virtual void set_disk_number(int num);
        virtual int get_disk_number() const; */
        virtual void set_disk_status(drive_status_t status) { this->status = status; };
        virtual drive_status_t get_disk_status() const { return status; };
        virtual void set_key(uint64_t k) { key = k; };
        virtual uint64_t get_key() const { return key; };
};