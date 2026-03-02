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

        // Handle drop position events, which track the mouse pos over window during drag/drop.
        // and update the hover state accordingly.
        virtual bool handle_mouse_event(const SDL_Event& event) override {
            if (!active || !visible) return(false);
            if (event.type == SDL_EVENT_DROP_POSITION) {
                float mouse_x = event.drop.x;
                float mouse_y = event.drop.y;
                
                // Check if mouse is within tile bounds
                bool is_inside = (mouse_x >= tp.x && mouse_x <= tp.x + tp.w &&
                                mouse_y >= tp.y && mouse_y <= tp.y + tp.h);
                
                // If we were hovering but mouse is now outside, or
                // we weren't hovering but mouse is now inside, trigger hover change
                if (is_hovering != is_inside) {
                    is_hovering = is_inside;
                    on_hover_changed(is_hovering);
                }
                return true;
                // others may care about same event
            } else return Button_t::handle_mouse_event(event);           
        }
    
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