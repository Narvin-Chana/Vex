#pragma once

#include <span>
#include <string>
#include <vector>

#include <Vex/RHI/RHIBindings.h>

namespace vex
{

struct ShaderResourceContext
{
    std::span<RHITextureBinding> textures;
    std::span<RHIBufferBinding> buffers;

    // The nth dword after which the root/push constants contain bindless indices (instead of local constants).
    u32 rootConstantBindlessSectionStartIndex = 0;

    std::vector<std::string> GenerateShaderBindings() const;
};

} // namespace vex
