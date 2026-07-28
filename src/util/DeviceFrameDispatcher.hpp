#pragma once

#include <functional>
#include <vector>

class DeviceFrameDispatcher {
    
public:
    using EventHandler = std::function<bool ()>;

    DeviceFrameDispatcher();
    ~DeviceFrameDispatcher();

    void registerHandler(EventHandler handler);
    void dispatch();

protected:
    std::vector<EventHandler> handlers;

};