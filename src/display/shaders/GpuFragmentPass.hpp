#pragma once

#include <SDL3/SDL.h>

// A custom fragment shader and the SDL GPU render state that applies it.
// Used for the main-render NTSC decode and for post-process passes (CRT).
struct GpuFragmentPass {
    enum Status { NOT_CREATED, NO_GPU, NO_SHADER, NO_STATE, READY };

    SDL_GPUShader *shader = nullptr;
    SDL_GPURenderState *state = nullptr;
    Status status = NOT_CREATED;

    // Loads shaders/<name>.frag.{metal|dxil|spv}, picked by the device's shader formats.
    bool create(SDL_Renderer *renderer, SDL_GPUDevice *device, const char *name,
                Uint32 num_samplers, Uint32 num_uniform_buffers);
    void destroy(SDL_GPUDevice *device);
    bool ok() const { return state != nullptr; }
};
