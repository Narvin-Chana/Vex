#pragma once

#include <span>
#include <string>
#include <vector>

#include <Vex/Bindings.h>
#include <Vex/RHIBindings.h>
#include <Vex/TextureSampler.h>

namespace vex
{

struct ShaderResourceContext
{
    // All resources that this shader requires.
    std::span<RHITextureBinding> textures;
    std::span<RHIBufferBinding> buffers;

    std::optional<ConstantBinding> constantBinding;

    // Static samplers to include via codegen.
    std::span<const TextureSampler> samplers;

    std::vector<std::string> GenerateShaderBindings() const;
};

} // namespace vex
