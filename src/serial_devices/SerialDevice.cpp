#include <SDL3/SDL.h>

#include "SerialDevice.hpp"


// This is the main device thread. This is a bare C routine that calls the SerialDevice object.
int SDLCALL SerialDeviceThreadHandler(void *data) {
    SerialDevice *device = (SerialDevice *)data;
    device->device_loop();
    return 0;
}

SerialDevice::SerialDevice(const char *name, const char *port_id) {
    this->name = name ? name : "SerialDevice";
    this->port_id = port_id ? port_id : "UNK";

    // start the thread, and call it
    this->thread = SDL_CreateThread(SerialDeviceThreadHandler, name, (void *) this);
}

SerialDevice::~SerialDevice() {
    // Only shut down if not already done by derived class
    if (thread) {
        SDL_Log("SerialDevice: %s shutting down", name);
        SerialMessage msg = {MESSAGE_SHUTDOWN, 0};
        q_host.send(msg);
        SDL_WaitThread(thread, NULL);
        thread = nullptr;
    }
}