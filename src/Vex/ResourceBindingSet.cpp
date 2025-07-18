#include "ResourceBindingSet.h"

#include "GfxBackend.h"
#include "RHI/RHITexture.h"

namespace vex
{

void ResourceBindingSet::ValidateBindings() const
{
    ResourceBinding::ValidateResourceBindings(reads, ResourceUsage::Read);
    ResourceBinding::ValidateResourceBindings(writes, ResourceUsage::UnorderedAccess);
}

void ResourceBindingSet::CollectRHIResources(GfxBackend& backend,
                                             std::span<const ResourceBinding> resources,
                                             std::vector<RHITextureBinding>& textureBindings,
                                             std::vector<RHIBufferBinding>& bufferBindings,
                                             ResourceUsage::Type usage)
{
    textureBindings.reserve(textureBindings.size() + resources.size());
    bufferBindings.reserve(bufferBindings.size() + resources.size());

    for (const auto& binding : resources)
    {
        if (binding.IsTexture())
        {
            auto& texture = backend.GetRHITexture(binding.texture.handle);
            textureBindings.emplace_back(binding, usage, &texture);
        }

        if (binding.IsBuffer())
        {
            auto& buffer = backend.GetRHIBuffer(binding.buffer.handle);
            bufferBindings.emplace_back(binding, usage, &buffer);
        }
    }
}

} // namespace vex