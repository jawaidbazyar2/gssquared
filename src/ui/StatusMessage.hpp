#pragma once

#include "Container.hpp"

class StatusMessage_t : public Container_t {
protected:
    std::string headsUpMessageText;
    int headsUpMessageCount = 0;
    int msg_x = 0;
public:
    StatusMessage_t(UIContext *ctx);
    void update() override;
    void layout() override;
    void render() override;
    bool handle_mouse_event(const SDL_Event& event) override;
    void trigger(const std::string& message);
};