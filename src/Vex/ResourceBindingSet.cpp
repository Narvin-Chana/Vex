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

void ResourceBindingSet::CollectResources(GfxBackend& backend,
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
ResourceBindingSet::ResourceBindingSet()
{
}

ResourceBindingSet& ResourceBindingSet::SetWriteBindings(std::span<const ResourceBinding> bindings)
{
    writes = std::vector(bindings.begin(), bindings.end());
    return *this;
}

ResourceBindingSet& ResourceBindingSet::SetReadBindings(std::span<const ResourceBinding> bindings)
{
    reads = std::vector(bindings.begin(), bindings.end());
    return *this;
}

ResourceBindingSet& ResourceBindingSet::SetConstantsBindings(std::span<const ConstantBinding> bindings)
{
    constants = std::vector(bindings.begin(), bindings.end());
    return *this;
}

void ResourceBindingSet::CollectWriteBindings(GfxBackend& backend,
                                              std::vector<RHITextureBinding>& textureBindings,
                                              std::vector<RHIBufferBinding>& bufferBindings) const
{
    CollectResources(backend, writes, textureBindings, bufferBindings, ResourceUsage::UnorderedAccess);
}

void ResourceBindingSet::CollectReadBindings(GfxBackend& backend,
                                             std::vector<RHITextureBinding>& textureBindings,
                                             std::vector<RHIBufferBinding>& bufferBindings) const
{
    CollectResources(backend, reads, textureBindings, bufferBindings, ResourceUsage::Read);
}

} // namespace vex