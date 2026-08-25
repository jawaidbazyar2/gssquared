#pragma once

#include <cstddef>
#include <functional>
#include <vector>

class DeviceFrameDispatcher {
    
public:
    using EventHandler = std::function<bool ()>;

    DeviceFrameDispatcher();
    ~DeviceFrameDispatcher();

    size_t registerHandler(EventHandler handler);
    void unregisterHandler(size_t id);
    void dispatch();

protected:
    std::vector<EventHandler> handlers;

};