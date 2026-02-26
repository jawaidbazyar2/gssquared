#pragma once

#include "Button.hpp"
#include "util/mount.hpp"

// Or should we just add an Interface?

class StorageButton : public Button_t {
    protected:
        storage_key_t key;
        drive_status_t status = { .is_mounted = false,  .filename = "", .motor_on = false, .position = 0, .is_modified = false};
    public:
    // use same constructors as Button_t.
        using Button_t::Button_t;
    
        // Disk state setters and getters
        /* virtual void set_disk_slot(int slot);
        virtual int get_disk_slot() const;
        virtual void set_disk_number(int num);
        virtual int get_disk_number() const; */
        inline virtual void set_disk_status(drive_status_t status) { this->status = status; };
        inline virtual drive_status_t get_disk_status() const { return status; };
        inline virtual void set_key(storage_key_t k) { key = k; };
        inline virtual storage_key_t get_key() const { return key; };
};