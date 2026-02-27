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
                            [](const ASBinding& asBinding)
                            {
                                // no-op
                            } },
                   binding.binding);
    }
}

RHIDrawResources ResourceBindingUtils::CollectRHIDrawResources(Graphics& graphics,
                                                               Span<const TextureBinding> renderTargets,
                                                               std::optional<TextureBinding> depthStencil,
                                                               std::optional<DepthStencilState> depthStencilState)
{
    RHIDrawResources drawResources;
    drawResources.renderTargets.reserve(renderTargets.size());

    std::size_t totalSize = renderTargets.size() + static_cast<std::size_t>(depthStencil.has_value());

    for (const auto& renderTarget : renderTargets)
    {
        auto& texture = graphics.GetRHITexture(renderTarget.texture.handle);
        drawResources.renderTargets.emplace_back(renderTarget, texture);
    }
    if (depthStencil.has_value())
    {
        auto& texture = graphics.GetRHITexture(depthStencil->texture.handle);
        drawResources.depthStencil = RHITextureBinding{ .binding = *depthStencil, .texture = texture };
    }

    return drawResources;
}

} // namespace vex