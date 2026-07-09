#pragma once

#include <functional>
#include <vector>

#include "Container.hpp"
#include "StorageButton.hpp"
#include "util/mount.hpp"

using StorageButtonClickHandler = std::function<void(StorageButton *button, const SDL_Event& event)>;

class DrivesOSD_t : public Container_t {
protected:
    int drives_x = 0;
    int drives_y = 0;

    std::vector<StorageButton *> buttons;
    Style_t button_style;
    StorageButtonClickHandler click_handler;

public:
    DrivesOSD_t(UIContext *ctx, const Style_t& initial_style);
    ~DrivesOSD_t() override;

    void set_button_style(const Style_t& style) { button_style = style; }
    void set_click_handler(StorageButtonClickHandler handler) { click_handler = std::move(handler); }

    /** Clear owned drive buttons and rebuild from specs. */
    void rebuild(const std::vector<drive_spec_t>& drives);

    void layout() override;
};
