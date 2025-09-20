#include "ResourceBindingUtils.h"

#include <Vex/Containers/Utils.h>
#include <Vex/GfxBackend.h>

#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{

RHIBufferBarrier ResourceBindingUtils::CreateBarrierFromRHIBinding(RHIBarrierSync dstSync,
                                                                   const RHIBufferBinding& rhiBufferBinding)
{
    const auto& [binding, buffer] = rhiBufferBinding;

    // For now bindings are only used for Graphics passes (draws).
    // TODO: In the future we could improve this by considering individual shader stages (eg: pixel shader).
    RHIBarrierAccess dstAccess = RHIBarrierAccess::NoAccess;
    switch (binding.usage)
    {
    case BufferBindingUsage::ConstantBuffer:
        dstAccess = RHIBarrierAccess::UniformRead;
        break;
    case BufferBindingUsage::StructuredBuffer:
    case BufferBindingUsage::ByteAddressBuffer:
        dstAccess = RHIBarrierAccess::ShaderRead;
        break;
    case BufferBindingUsage::RWStructuredBuffer:
    case BufferBindingUsage::RWByteAddressBuffer:
        dstAccess = RHIBarrierAccess::ShaderReadWrite;
        break;
    default:
        VEX_LOG(Fatal, "Invalid buffer binding!");
    }

    return RHIBufferBarrier{ buffer, dstSync, dstAccess };
}

RHITextureBarrier ResourceBindingUtils::CreateBarrierFromRHIBinding(RHIBarrierSync dstSync,
                                                                    const RHITextureBinding& rhiTextureBinding)
{
    const auto& [binding, texture] = rhiTextureBinding;

    // For now bindings are only used for Graphics passes (draws).
    // TODO: In the future we could improve this by considering individual shader stages (eg: pixel shader).
    RHIBarrierAccess dstAccess = RHIBarrierAccess::NoAccess;
    RHITextureLayout dstLayout = RHITextureLayout::Common;
    switch (binding.usage)
    {
    case TextureBindingUsage::ShaderRead:
        dstAccess = RHIBarrierAccess::ShaderRead;
        dstLayout = RHITextureLayout::ShaderResource;
        break;
    case TextureBindingUsage::ShaderReadWrite:
        dstAccess = RHIBarrierAccess::ShaderReadWrite;
        dstLayout = RHITextureLayout::UnorderedAccess;
        break;
    default:
        VEX_LOG(Fatal, "Invalid texture binding!");
    }

    return RHITextureBarrier{ texture, dstSync, dstAccess, dstLayout };
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

RHIDrawResources ResourceBindingUtils::CollectRHIDrawResourcesAndBarriers(GfxBackend& backend,
                                                                          std::span<const TextureBinding> renderTargets,
                                                                          std::optional<TextureBinding> depthStencil,
                                                                          std::vector<RHITextureBarrier>& barriers)
{
    RHIDrawResources drawResources;
    drawResources.renderTargets.reserve(renderTargets.size());

    std::size_t totalSize = renderTargets.size() + static_cast<std::size_t>(depthStencil.has_value());
    barriers.reserve(barriers.size() + totalSize);

    for (const auto& renderTarget : renderTargets)
    {
        auto& texture = backend.GetRHITexture(renderTarget.texture.handle);
        barriers.push_back(RHITextureBarrier{
            texture,
            RHIBarrierSync::RenderTarget,
            // This technically doesn't support Vk's RenderTargetRead...
            RHIBarrierAccess::RenderTarget,
            RHITextureLayout::RenderTarget,
        });
        drawResources.renderTargets.emplace_back(renderTarget, texture);
    }
    if (depthStencil.has_value())
    {
        auto& texture = backend.GetRHITexture(depthStencil->texture.handle);

        // TODO: This deduces depth and/or stencil from the texture's format, ideally we'd pass this info along in
        // the binding somehow.
        bool supportsStencil = DoesFormatSupportStencil(texture.GetDescription().format);

        barriers.push_back(RHITextureBarrier{
            texture,
            supportsStencil ? RHIBarrierSync::DepthStencil : RHIBarrierSync::Depth,
            // TODO: What about if we want to do DepthRead? Would require a flag in the binding!
            RHIBarrierAccess::DepthStencil,
            RHITextureLayout::DepthStencilWrite,
        });
        drawResources.depthStencil = RHITextureBinding{ .binding = *depthStencil, .texture = texture };
    }

    return drawResources;
}

} // namespace vex