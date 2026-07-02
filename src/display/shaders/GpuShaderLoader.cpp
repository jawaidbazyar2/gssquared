#include "display/shaders/GpuShaderLoader.hpp"

#include "gs2.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

constexpr uintmax_t MAX_SHADER_SOURCE_BYTES = 256 * 1024;

} // namespace

bool load_resource_text(const char *relative_path, std::string &out) {
    out.clear();

    if (!relative_path || relative_path[0] == '\0') {
        fprintf(stderr, "load_resource_text: empty path\n");
        return false;
    }

    std::string full_path = gs2_app_values.base_path;
    full_path.append(relative_path);

    if (!std::filesystem::exists(full_path)) {
        fprintf(stderr, "load_resource_text: file not found: %s\n", full_path.c_str());
        return false;
    }

    const uintmax_t file_size = std::filesystem::file_size(full_path);
    if (file_size == 0) {
        fprintf(stderr, "load_resource_text: file is empty: %s\n", full_path.c_str());
        return false;
    }
    if (file_size > MAX_SHADER_SOURCE_BYTES) {
        fprintf(stderr, "load_resource_text: file too large (%ju bytes): %s\n",
            (uintmax_t)file_size, full_path.c_str());
        return false;
    }

    std::ifstream file(full_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "load_resource_text: failed to open: %s\n", full_path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (!file.good() && !file.eof()) {
        fprintf(stderr, "load_resource_text: failed to read: %s\n", full_path.c_str());
        return false;
    }

    out = buffer.str();
    return true;
}

SDL_GPUShader *create_gpu_shader_from_resource(
    SDL_GPUDevice *device,
    const char *resource_path,
    SDL_GPUShaderStage stage,
    Uint32 num_samplers,
    Uint32 num_uniform_buffers,
    const char *entrypoint)
{
    if (!device) {
        fprintf(stderr, "create_gpu_shader_from_resource: null GPU device\n");
        return nullptr;
    }

    std::string source;
    if (!load_resource_text(resource_path, source)) {
        return nullptr;
    }

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (!(formats & SDL_GPU_SHADERFORMAT_MSL)) {
        fprintf(stderr, "create_gpu_shader_from_resource: MSL not supported by GPU device\n");
        return nullptr;
    }

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.format = SDL_GPU_SHADERFORMAT_MSL;
    info.code = reinterpret_cast<const Uint8 *>(source.data());
    info.code_size = source.size();
    info.num_samplers = num_samplers;
    info.num_uniform_buffers = num_uniform_buffers;
    info.stage = stage;
    info.entrypoint = entrypoint;

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        fprintf(stderr, "create_gpu_shader_from_resource(%s): %s\n",
            resource_path, SDL_GetError());
    }
    return shader;
}
