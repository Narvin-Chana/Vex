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
                .subresource =
                    TextureSubresource{
                        .startMip = textureBinding.subresource.startMip,
                        .mipCount = textureBinding.subresource.GetMipCount(texture.GetDesc()),
                        .startSlice = textureBinding.subresource.startSlice,
                        .sliceCount = textureBinding.subresource.GetSliceCount(texture.GetDesc()),
                        .aspect = textureBinding.subresource.GetAspect(texture.GetDesc()),
                    },
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
                .region =
                    BufferRegion{
                        .byteOffset = bufferBinding.region.byteOffset,
                        .byteSize = bufferBinding.region.GetByteSize(buffer.GetDesc()),
                    },
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
                    .subresource =
                        TextureSubresource{
                            .startMip = subresource.startMip,
                            .mipCount = subresource.GetMipCount(rhiTexture.GetDesc()),
                            .startSlice = subresource.startSlice,
                            .sliceCount = subresource.GetSliceCount(rhiTexture.GetDesc()),
                            .aspect = subresource.GetAspect(rhiTexture.GetDesc()),
                        },
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
                    .subresource =
                        TextureSubresource{
                            .startMip = depthStencil->subresource.startMip,
                            .mipCount = depthStencil->subresource.GetMipCount(texture.GetDesc()),
                            .startSlice = depthStencil->subresource.startSlice,
                            .sliceCount = depthStencil->subresource.GetSliceCount(texture.GetDesc()),
                            .aspect = depthStencil->subresource.GetAspect(texture.GetDesc()),
                        },
                },
        };
    }
    return drawResources;
}

} // namespace vex