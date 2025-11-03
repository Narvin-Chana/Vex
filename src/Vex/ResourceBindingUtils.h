#pragma once

#include <span>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{
class GfxBackend;

struct ResourceBindingUtils
{
    // This combines the behavior of CollectRHITextures and CollectRHIBuffers to have a simple central
    // collection when there are buffers and textures in the same collection of ResourceBindings
    static void CollectRHIResources(GfxBackend& backend,
                                    std::span<const ResourceBinding> resources,
                                    std::vector<RHITextureBinding>& textureBindings,
                                    std::vector<RHIBufferBinding>& bufferBindings);

    // Collects draw textures from a set of render targets and a depth stencil
    static RHIDrawResources CollectRHIDrawResourcesAndBarriers(GfxBackend& backend,
                                                               std::span<const TextureBinding> renderTargets,
                                                               std::optional<TextureBinding> depthStencil,
                                                               std::vector<RHITextureBarrier>& barriers);

    static RHIBufferBarrier CreateBarrierFromRHIBinding(RHIBarrierSync stage, const RHIBufferBinding& rhiBufferBinding);
    static RHITextureBarrier CreateBarrierFromRHIBinding(RHIBarrierSync stage,
                                                         const RHITextureBinding& rhiTextureBinding);
};

} // namespace vex
