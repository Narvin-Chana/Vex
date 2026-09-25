#pragma once

#include <Vex/Bindings.h>
#include <Vex/Containers/Span.h>
#include <Vex/GraphicsPipeline.h>

#include <RHI/RHIBindings.h>

namespace vex
{

class Graphics;

struct ResourceBindingUtils
{
    static RHITextureView GetRHITextureView(Graphics& graphics, const TextureBinding& textureBinding);
    static RHIBufferView GetRHIBufferView(Graphics& graphics, const BufferBinding& bufferBinding);
    static RHIIndexBufferView GetRHIIndexBufferView(Graphics& graphics, const IndexBufferBinding& indexBufferBinding);

    // This combines the behavior of CollectRHITextures and CollectRHIBuffers to have a simple central
    // collection when there are buffers and textures in the same collection of ResourceBindings.
    static void CollectRHIViews(Graphics& graphics,
                                Span<const ResourceBinding> resources,
                                Span<RHITextureView>& textureViews,
                                Span<RHIBufferView>& bufferViews);

    // Collects draw resources from a set of render targets and an optional depth stencil.
    static RHIDrawResources CollectRHIDrawResources(Graphics& graphics,
                                                    Span<const RenderTargetBinding> renderTargets,
                                                    const DepthStencilBinding* depthStencil);
};

} // namespace vex
