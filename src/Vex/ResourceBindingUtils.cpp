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
        .view = TextureViewDesc::Create(texture.GetDesc(),
                                        textureBinding.subresource,
                                        static_cast<TextureUsage>(textureBinding.usage),
                                        textureBinding.isSRGB,
                                        textureBinding.viewTypeOverride),
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
                .strideByteSize = bufferBinding.strideByteSize,
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
        .region =
            BufferRegion{
                .byteOffset = indexBufferBinding.region.byteOffset,
                .byteSize = indexBufferBinding.region.GetByteSize(buffer.GetDesc()),
            },
        .format = indexBufferBinding.format,
    };
}

RHIDrawResources ResourceBindingUtils::CollectRHIDrawResources(Graphics& graphics,
                                                               Span<const RenderTargetBinding> renderTargets,
                                                               const DepthStencilBinding* depthStencil)
{
    RHIDrawResources drawResources;
    for (const auto& rt : renderTargets)
    {
        RHITexture& rhiTexture = graphics.GetRHITexture(rt.texture.handle);
        drawResources.renderTargets.push_back(RHIRenderTargetView{
            .texture = rhiTexture,
            .view = TextureViewDesc::Create(rhiTexture.GetDesc(),
                                            TextureSubresource{
                                                .startMip = rt.mip,
                                                .mipCount = 1,
                                                .startSlice = rt.startDepthSlice,
                                                .sliceCount = rt.depthSliceCount,
                                                .aspect = TextureAspect::Color,
                                            },
                                            TextureUsage::RenderTarget,
                                            rt.isSRGB),
        });
    }
    if (depthStencil)
    {
        RHITexture& texture = graphics.GetRHITexture(depthStencil->texture.handle);
        drawResources.depthStencil = RHIDepthStencilView{
            .texture = texture,
            .view = TextureViewDesc::Create(texture.GetDesc(),
                                            TextureSubresource{
                                                .startMip = depthStencil->mip,
                                                .mipCount = 1,
                                                .startSlice = depthStencil->startDepthSlice,
                                                .sliceCount = depthStencil->depthSliceCount,
                                                .aspect = depthStencil->aspect,
                                            },
                                            TextureUsage::DepthStencil),
        };
    }
    return drawResources;
}

} // namespace vex