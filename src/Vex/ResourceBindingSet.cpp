#include "ResourceBindingSet.h"

#include <Vex/GfxBackend.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{

void ResourceBindingSet::ValidateBindings() const
{
    ResourceBinding::ValidateResourceBindings(reads,
                                              TextureUsage::ShaderRead,
                                              BufferUsage::UniformBuffer | BufferUsage::GenericBuffer);
    ResourceBinding::ValidateResourceBindings(writes, TextureUsage::ShaderReadWrite, BufferUsage::ReadWriteBuffer);
}

void ResourceBindingSet::CollectRHIResources(GfxBackend& backend,
                                             std::span<const ResourceBinding> resources,
                                             std::vector<RHITextureBinding>& textureBindings,
                                             std::vector<RHIBufferBinding>& bufferBindings,
                                             TextureUsage::Type textureUsage,
                                             BufferUsage::Flags bufferUsage)
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

        if (binding.IsBuffer() && IsBindingUsageCompatibleWithBufferUsage(bufferUsage, binding.bufferUsage))
        {
            auto& buffer = backend.GetRHIBuffer(binding.buffer.handle);
            bufferBindings.emplace_back(binding, &buffer);
        }
    }
}

} // namespace vex