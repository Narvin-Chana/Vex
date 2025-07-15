#include "ResourceBindingSet.h"

#include <Vex/GfxBackend.h>
#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHITexture.h>

namespace vex
{

void ResourceBindingSet::ValidateBindings() const
{
    ResourceBinding::ValidateResourceBindings(reads, TextureUsage::Read, BufferUsage::Read);
    ResourceBinding::ValidateResourceBindings(writes, TextureUsage::UnorderedAccess, BufferUsage::UnorderedAccess);
}

void ResourceBindingSet::CollectRHIResources(GfxBackend& backend,
                                             std::span<const ResourceBinding> resources,
                                             std::vector<RHITextureBinding>& textureBindings,
                                             std::vector<RHIBufferBinding>& bufferBindings,
                                             TextureUsage::Type textureUsage,
                                             BufferUsage::Type bufferUsage)
{
    textureBindings.reserve(textureBindings.size() + resources.size());
    bufferBindings.reserve(bufferBindings.size() + resources.size());

    for (const auto& binding : resources)
    {
        if (binding.IsTexture())
        {
            auto& texture = backend.GetRHITexture(binding.texture.handle);
            textureBindings.emplace_back(binding, textureUsage, &texture);
        }

        if (binding.IsBuffer())
        {
            auto& buffer = backend.GetRHIBuffer(binding.buffer.handle);
            bufferBindings.emplace_back(binding, bufferUsage, &buffer);
        }
    }
}

} // namespace vex