#pragma once

#include "Container.hpp"
#include "NClock.hpp"

struct computer_t;

class SpeedSelect_t : public Container_t {
protected:
    computer_t *computer = nullptr;
    NClock *clock = nullptr;
public:
    SpeedSelect_t(UIContext *ctx, const Style_t& initial_style = Style_t(), computer_t *computer = nullptr);
    ~SpeedSelect_t();
    virtual void set_visible(bool visible) override;
    virtual void render() override;
    virtual bool handle_mouse_event(const SDL_Event& event) override;
};
