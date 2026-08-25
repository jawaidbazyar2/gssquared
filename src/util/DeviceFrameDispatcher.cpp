#include "DeviceFrameDispatcher.hpp"

#include <utility>

DeviceFrameDispatcher::DeviceFrameDispatcher() {
}

DeviceFrameDispatcher::~DeviceFrameDispatcher() {
}

size_t DeviceFrameDispatcher::registerHandler(EventHandler handler) {
    handlers.push_back(std::move(handler));
    return handlers.size() - 1;
}

void DeviceFrameDispatcher::unregisterHandler(size_t id) {
    if (id < handlers.size()) {
        handlers[id] = nullptr;
    }
}

void DeviceFrameDispatcher::dispatch() {
    for (auto& handler : handlers) {
        if (handler) {
            handler();
        }
    }
}