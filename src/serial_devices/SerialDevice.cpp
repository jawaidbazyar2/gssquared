#include <SDL3/SDL.h>

#include "SerialDevice.hpp"


// This is the main device thread. This is a bare C routine that calls the SerialDevice object.
int SDLCALL SerialDeviceThreadHandler(void *data) {
    SerialDevice *device = (SerialDevice *)data;
    device->device_loop();
    return 0;
}

SerialDevice::SerialDevice(const char *name) {
    this->name = name;

    // start the thread, and call it
    SDL_CreateThread(SerialDeviceThreadHandler, name, (void *) this);
}
