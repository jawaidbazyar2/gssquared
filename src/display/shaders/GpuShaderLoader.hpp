#pragma once

#include <SDL3/SDL_gpu.h>
#include <cstdint>
#include <string>
#include <vector>

// Load a UTF-8 text file relative to gs2_app_values.base_path.
bool load_resource_text(const char *relative_path, std::string &out);

// Load a binary file relative to gs2_app_values.base_path (DXIL, etc.).
bool load_resource_bytes(const char *relative_path, std::vector<uint8_t> &out);

// Load shader code from a resource path and create an SDL_GPUShader.
// .metal files are MSL source (entrypoint nullptr uses SDL/Metal default "main0").
// .dxil files are DXIL bytecode (pass entrypoint "main").
SDL_GPUShader *create_gpu_shader_from_resource(
    SDL_GPUDevice *device,
    const char *resource_path,
    SDL_GPUShaderStage stage,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers,
    const char *entrypoint = nullptr);
