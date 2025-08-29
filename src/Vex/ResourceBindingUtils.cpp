#include "ResourceBindingUtils.h"

#include <Vex/Containers/Utils.h>
#include <Vex/GfxBackend.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{

RHITextureState::Type ResourceBindingUtils::TextureBindingUsageToState(TextureUsage::Type usage)
{
    switch (usage)
    {
    case TextureUsage::DepthStencil:
        return RHITextureState::DepthRead;
    case TextureUsage::ShaderRead:
        return RHITextureState::ShaderResource;
    case TextureUsage::ShaderReadWrite:
        return RHITextureState::ShaderReadWrite;
    case TextureUsage::RenderTarget:
        return RHITextureState::RenderTarget;
    default:
        VEX_LOG(Fatal, "TextureBindingUsage to RHITextureState not supported")
    }
    std::unreachable();
}

RHIBufferState::Flags ResourceBindingUtils::BufferBindingUsageToState(BufferBindingUsage usage)
{
    switch (usage)
    {
    case BufferBindingUsage::ConstantBuffer:
    case BufferBindingUsage::ByteAddressBuffer:
    case BufferBindingUsage::StructuredBuffer:
        return RHIBufferState::ShaderResource;
    case BufferBindingUsage::RWByteAddressBuffer:
    case BufferBindingUsage::RWStructuredBuffer:
        return RHIBufferState::ShaderReadWrite;
    default:
        VEX_LOG(Fatal, "BufferBindingUsage to RHIBufferState not supported")
    }
    std::unreachable();
}

void ResourceBindingUtils::ValidateResourceBindings(std::span<const ResourceBinding> bindings)
{
    for (const auto& resource : bindings)
    {
        std::visit(
            Visitor{
                [](const TextureBinding& binding) { binding.Validate(); },
                [](const BufferBinding& binding) { binding.Validate(); },
            },
            resource.binding);
    }
}

void ResourceBindingUtils::CollectRHIResources(GfxBackend& backend,
                                               std::span<const ResourceBinding> resources,
                                               std::vector<RHITextureBinding>& textureBindings,
                                               std::vector<RHIBufferBinding>& bufferBindings)
{
    for (const auto& binding : resources)
    {
        std::visit(Visitor{ [&](const BufferBinding& bufferBinding)
                            {
                                RHIBuffer& buffer = backend.GetRHIBuffer(bufferBinding.buffer.handle);
                                bufferBindings.emplace_back(bufferBinding, NonNullPtr(buffer));
                            },
                            [&](const TextureBinding& texBinding)
                            {
                                RHITexture& texture = backend.GetRHITexture(texBinding.texture.handle);
                                textureBindings.emplace_back(texBinding, NonNullPtr(texture));
                            } },
                   binding.binding);
    }
}

} // namespace vex