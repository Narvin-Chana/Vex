#include "ShaderResourceContext.h"

namespace vex
{

std::vector<std::string> ShaderResourceContext::GenerateShaderBindings() const
{
    // Accumulate only the types of resources that are required to be named, these will require codegen to be bound
    // correctly to our shader. This will keep only SRV and UAVs.
    std::vector<std::string> names;
    std::transform(textures.begin(),
                   textures.end(),
                   std::back_inserter(names),
                   [](const auto& texBinding) { return texBinding.binding.name; });

    std::transform(buffers.begin(),
                   buffers.end(),
                   std::back_inserter(names),
                   [](const auto& bufBinding) { return bufBinding.binding.name; });

    return names;
}

} // namespace vex