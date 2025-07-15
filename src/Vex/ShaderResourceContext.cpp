#include "ShaderResourceContext.h"

namespace vex
{

std::vector<std::string> ShaderResourceContext::GenerateShaderBindings() const
{
    // Accumulate only the types of resources that are required to be named, these will require codegen to be bound
    // correctly to our shader. This will keep only SRV and UAVs.
    std::vector<std::string> names;
    for (const auto& tex : textures)
    {
        if (tex.usage == TextureUsage::Read || tex.usage == TextureUsage::UnorderedAccess)
        {
            names.emplace_back(tex.binding.name);
        }
    }

    for (const auto& buf : buffers)
    {
        if (buf.usage == BufferUsage::Read || buf.usage == BufferUsage::UnorderedAccess)
        {
            names.emplace_back(buf.binding.name);
        }
    }

    return names;
}

} // namespace vex