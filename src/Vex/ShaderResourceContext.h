#pragma once

#include <span>
#include <string>
#include <vector>

#include <Vex/RHIBindings.h>
#include <Vex/TextureSampler.h>

namespace vex
{

struct ShaderResourceContext
{
    // All resources that this pass binds.
    std::span<RHITextureBinding> textures;
    std::span<RHIBufferBinding> buffers;

    // Static samplers to include via codegen.
    std::span<const TextureSampler> samplers;

    std::vector<std::string> GenerateShaderBindings() const;
};

} // namespace vex
