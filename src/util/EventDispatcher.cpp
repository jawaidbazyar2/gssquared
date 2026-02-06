#include <functional>
#include <vector>
#include <unordered_map>
#include <SDL3/SDL.h>

#include "EventDispatcher.hpp"
#include "debug.hpp"

void EventDispatcher::registerHandler(Uint32 eventType, EventHandler handler) {
    handlers[eventType].push_back(std::move(handler));
}

void EventDispatcher::registerHandler(const std::vector<Uint32>& eventTypes, EventHandler handler) {
    for (Uint32 type : eventTypes) {
        handlers[type].push_back(handler);
    }
}

// Dispatch an SDL event to all registered handlers
// Returns true if event was handled (consumed)
bool EventDispatcher::dispatch(const SDL_Event& event)
{
    bool handled = false;

    if (auto it = handlers.find(event.type); it != handlers.end()) {
        for (const auto& handler : it->second) {
            if (handler(event)) {
                handled = true;
                // You might want to break here if you want first-handler-wins
                break;
            }
        }
    }

    return handled;
}


