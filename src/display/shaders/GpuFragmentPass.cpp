#include "display/shaders/GpuFragmentPass.hpp"
#include "display/shaders/GpuShaderLoader.hpp"

#include <cstdio>
#include <string>

bool GpuFragmentPass::create(SDL_Renderer *renderer, SDL_GPUDevice *device, const char *name,
                             Uint32 num_samplers, Uint32 num_uniform_buffers)
{
    if (!device) {
        status = NO_GPU;
        return false;
    }

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device);
    std::string path = std::string("shaders/") + name + ".frag";
    const char *entrypoint = nullptr;
    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        path += ".metal";
        entrypoint = nullptr; // Metal default: main0
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        path += ".dxil";
        entrypoint = "main";
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        path += ".spv";
        entrypoint = "main";
    } else {
        printf("%s shader: no supported shader format (need MSL, DXIL, or SPIR-V)\n", name);
        status = NO_SHADER;
        return false;
    }

    shader = create_gpu_shader_from_resource(device, path.c_str(), SDL_GPU_SHADERSTAGE_FRAGMENT,
                                             num_samplers, num_uniform_buffers, entrypoint);
    if (!shader) {
        status = NO_SHADER;
        return false;
    }

    SDL_GPURenderStateDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.fragment_shader = shader;
    state = SDL_CreateGPURenderState(renderer, &desc);
    if (!state) {
        printf("%s shader: SDL_CreateGPURenderState failed: %s\n", name, SDL_GetError());
        SDL_ReleaseGPUShader(device, shader);
        shader = nullptr;
        status = NO_STATE;
        return false;
    }
    status = READY;
    return true;
}

void GpuFragmentPass::destroy(SDL_GPUDevice *device) {
    if (state) SDL_DestroyGPURenderState(state);
    if (shader && device) SDL_ReleaseGPUShader(device, shader);
    state = nullptr;
    shader = nullptr;
    status = NOT_CREATED;
}
