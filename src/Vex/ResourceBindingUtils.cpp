#include "ResourceBindingUtils.h"

#include <Vex/Graphics.h>
#include <Vex/Utility/Visitor.h>

namespace vex
{

void ResourceBindingUtils::CollectRHIResources(Graphics& graphics,
                                               Span<const ResourceBinding> resources,
                                               std::vector<RHITextureBinding>& textureBindings,
                                               std::vector<RHIBufferBinding>& bufferBindings)
{
    for (const auto& binding : resources)
    {
        std::visit(Visitor{ [&](const BufferBinding& bufferBinding)
                            {
                                RHIBuffer& buffer = graphics.GetRHIBuffer(bufferBinding.buffer.handle);
                                bufferBindings.emplace_back(bufferBinding, NonNullPtr(buffer));
                            },
                            [&](const TextureBinding& texBinding)
                            {
                                RHITexture& texture = graphics.GetRHITexture(texBinding.texture.handle);
                                textureBindings.emplace_back(texBinding, NonNullPtr(texture));
                            },
                            [](const AccelerationStructureBinding&)
                            {
                                // no-op
                            } },
                   binding.binding);
    }
}

RHIDrawResources ResourceBindingUtils::CollectRHIDrawResources(Graphics& graphics,
                                                               Span<const RenderTargetBinding> renderTargets,
                                                               const std::optional<DepthStencilBinding>& depthStencil)
{
    RHIDrawResources drawResources{
        .renderTargets = {},
        .depthStencil = depthStencil.has_value()
                            ? RHIDepthStencilBinding{ .binding = *depthStencil,
                                                      .texture = &graphics.GetRHITexture(depthStencil->texture.handle), }
                            : std::optional<RHIDepthStencilBinding>{},
    };
    for (const auto& renderTarget : renderTargets)
    {
        auto& texture = graphics.GetRHITexture(renderTarget.texture.handle);
        drawResources.renderTargets.emplace_back(renderTarget, &texture);
    }
    return drawResources;
}

} // namespace vex