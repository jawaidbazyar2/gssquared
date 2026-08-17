#include "display/shaders/GpuShaderLoader.hpp"

#include "gs2.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

constexpr uintmax_t MAX_SHADER_SOURCE_BYTES = 256 * 1024;

bool resolve_resource_path(const char *relative_path, const char *log_tag,
    std::string &full_path, uintmax_t &file_size)
{
    full_path.clear();
    file_size = 0;

    if (!relative_path || relative_path[0] == '\0') {
        fprintf(stderr, "%s: empty path\n", log_tag);
        return false;
    }

    full_path = gs2_app_values.base_path;
    full_path.append(relative_path);

    if (!std::filesystem::exists(full_path)) {
        fprintf(stderr, "%s: file not found: %s\n", log_tag, full_path.c_str());
        return false;
    }

    file_size = std::filesystem::file_size(full_path);
    if (file_size == 0) {
        fprintf(stderr, "%s: file is empty: %s\n", log_tag, full_path.c_str());
        return false;
    }
    if (file_size > MAX_SHADER_SOURCE_BYTES) {
        fprintf(stderr, "%s: file too large (%ju bytes): %s\n",
            log_tag, (uintmax_t)file_size, full_path.c_str());
        return false;
    }
    return true;
}

bool path_ends_with(const char *path, const char *suffix) {
    if (!path || !suffix) {
        return false;
    }
    const size_t path_len = strlen(path);
    const size_t suffix_len = strlen(suffix);
    if (suffix_len > path_len) {
        return false;
    }
    return strcmp(path + (path_len - suffix_len), suffix) == 0;
}

} // namespace

bool load_resource_text(const char *relative_path, std::string &out) {
    out.clear();

    std::string full_path;
    uintmax_t file_size = 0;
    if (!resolve_resource_path(relative_path, "load_resource_text", full_path, file_size)) {
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

bool load_resource_bytes(const char *relative_path, std::vector<uint8_t> &out) {
    out.clear();

    std::string full_path;
    uintmax_t file_size = 0;
    if (!resolve_resource_path(relative_path, "load_resource_bytes", full_path, file_size)) {
        return false;
    }

    std::ifstream file(full_path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "load_resource_bytes: failed to open: %s\n", full_path.c_str());
        return false;
    }

    out.resize(static_cast<size_t>(file_size));
    file.read(reinterpret_cast<char *>(out.data()), static_cast<std::streamsize>(file_size));
    if (!file) {
        fprintf(stderr, "load_resource_bytes: failed to read: %s\n", full_path.c_str());
        out.clear();
        return false;
    }
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

    const bool is_dxil = path_ends_with(resource_path, ".dxil");
    const SDL_GPUShaderFormat format = is_dxil
        ? SDL_GPU_SHADERFORMAT_DXIL
        : SDL_GPU_SHADERFORMAT_MSL;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    if (!(formats & format)) {
        fprintf(stderr, "create_gpu_shader_from_resource: %s not supported by GPU device\n",
            is_dxil ? "DXIL" : "MSL");
        return nullptr;
    }

    std::string text_source;
    std::vector<uint8_t> binary_source;
    const Uint8 *code = nullptr;
    size_t code_size = 0;

    if (is_dxil) {
        if (!load_resource_bytes(resource_path, binary_source)) {
            return nullptr;
        }
        code = binary_source.data();
        code_size = binary_source.size();
    } else {
        if (!load_resource_text(resource_path, text_source)) {
            return nullptr;
        }
        code = reinterpret_cast<const Uint8 *>(text_source.data());
        code_size = text_source.size();
    }

    SDL_GPUShaderCreateInfo info;
    SDL_zero(info);
    info.format = format;
    info.code = code;
    info.code_size = code_size;
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
