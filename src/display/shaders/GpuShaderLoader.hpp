#pragma once

#include <SDL3/SDL_gpu.h>
#include <string>

// Load a UTF-8 text file relative to gs2_app_values.base_path.
bool load_resource_text(const char *relative_path, std::string &out);

// Load MSL source from a resource path and create an SDL_GPUShader.
// entrypoint: nullptr uses SDL/Metal default ("main0").
SDL_GPUShader *create_gpu_shader_from_resource(
    SDL_GPUDevice *device,
    const char *resource_path,
    SDL_GPUShaderStage stage,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers,
    const char *entrypoint = nullptr);
