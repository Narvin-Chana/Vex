#pragma once

#include <Vex/Containers/Span.h>
#include <Vex/GraphicsPipeline.h>

#include <RHI/RHIBarrier.h>
#include <RHI/RHIBindings.h>
#include <RHI/RHIBuffer.h>
#include <RHI/RHITexture.h>

namespace vex
{

class Graphics;

struct ResourceBindingUtils
{
    // This combines the behavior of CollectRHITextures and CollectRHIBuffers to have a simple central
    // collection when there are buffers and textures in the same collection of ResourceBindings
    static void CollectRHIResources(Graphics& graphics,
                                    Span<const ResourceBinding> resources,
                                    std::vector<RHITextureBinding>& textureBindings,
                                    std::vector<RHIBufferBinding>& bufferBindings);

    // Collects draw textures from a set of render targets and a depth stencil
    static RHIDrawResources CollectRHIDrawResources(Graphics& graphics,
                                                    Span<const TextureBinding> renderTargets,
                                                    std::optional<TextureBinding> depthStencil);
};

} // namespace vex
