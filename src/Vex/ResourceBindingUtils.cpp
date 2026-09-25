#include "ResourceBindingUtils.h"

#include <Vex/Graphics.h>
#include <Vex/Utility/Visitor.h>

namespace vex
{

RHITextureView ResourceBindingUtils::GetRHITextureView(Graphics& graphics, const TextureBinding& textureBinding)
{
    RHITexture& texture = graphics.GetRHITexture(textureBinding.texture.handle);
    return RHITextureView{
        .texture = texture,
        .view =
            TextureViewDesc{
                .viewType = TextureUtil::GetTextureViewType(textureBinding),
                .format = texture.GetDesc().format,
                .isSRGB = textureBinding.isSRGB,
                .usage = static_cast<TextureUsage>(textureBinding.usage),
                .subresource = textureBinding.subresource,
            },
    };
}

RHIBufferView ResourceBindingUtils::GetRHIBufferView(Graphics& graphics, const BufferBinding& bufferBinding)
{
    RHIBuffer& buffer = graphics.GetRHIBuffer(bufferBinding.buffer.handle);
    return RHIBufferView{
        .buffer = buffer,
        .view =
            BufferViewDesc{
                .usage = bufferBinding.usage,
                .region = bufferBinding.region,
                .strideByteSize = bufferBinding.strideByteSize.value_or(0),
                .isAccelerationStructure = buffer.GetDesc().usage.IsSet(BufferUsage::AccelerationStructure),
            },
    };
}

RHIIndexBufferView ResourceBindingUtils::GetRHIIndexBufferView(Graphics& graphics,
                                                               const IndexBufferBinding& indexBufferBinding)
{
    RHIBuffer& buffer = graphics.GetRHIBuffer(indexBufferBinding.buffer.handle);
    return RHIIndexBufferView{
        .buffer = buffer,
        .region = indexBufferBinding.region,
        .format = indexBufferBinding.format,
    };
}

void ResourceBindingUtils::CollectRHIViews(Graphics& graphics,
                                           Span<const ResourceBinding> resources,
                                           Span<RHITextureView>& textureViews,
                                           Span<RHIBufferView>& bufferViews)
{
    VEX_ASSERT(textureViews.size() + bufferViews.size() >= resources.size(),
               "Texture and buffer view spans must be large enough to gather resources.");
    for (u64 i = 0; i < resources.size(); ++i)
    {
        const auto& binding = resources[i];
        std::visit(Visitor{ [&](const BufferBinding& bufferBinding)
                            { bufferViews[i] = GetRHIBufferView(graphics, bufferBinding); },
                            [&](const TextureBinding& textureBinding)
                            { textureViews[i] = GetRHITextureView(graphics, textureBinding); },
                            [](const AccelerationStructureBinding&)
                            {
                                // no-op
                            } },
                   binding.binding);
    }
}

RHIDrawResources ResourceBindingUtils::CollectRHIDrawResources(Graphics& graphics,
                                                               Span<const RenderTargetBinding> renderTargets,
                                                               const DepthStencilBinding* depthStencil)
{
    RHIDrawResources drawResources;
    for (const auto& [texture, subresource, isSRGB] : renderTargets)
    {
        RHITexture& rhiTexture = graphics.GetRHITexture(texture.handle);
        drawResources.renderTargets.push_back(RHIRenderTargetView{
            .texture = rhiTexture,
            .view =
                TextureViewDesc{
                    .viewType = TextureUtil::GetTextureViewType(rhiTexture.GetDesc(), std::nullopt),
                    .format = rhiTexture.GetDesc().format,
                    .isSRGB = isSRGB,
                    .usage = TextureUsage::RenderTarget,
                    .subresource = subresource,
                },
        });
    }
    if (depthStencil)
    {
        RHITexture& texture = graphics.GetRHITexture(depthStencil->texture.handle);
        drawResources.depthStencil = RHIDepthStencilView{
            .texture = texture,
            .view =
                TextureViewDesc{
                    .viewType = TextureUtil::GetTextureViewType(texture.GetDesc(), std::nullopt),
                    .format = texture.GetDesc().format,
                    .isSRGB = false,
                    .usage = TextureUsage::DepthStencil,
                    .subresource = depthStencil->subresource,
                },
        };
    }
    return drawResources;
}

} // namespace vex