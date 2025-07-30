#pragma once

#include <span>
#include <vector>

#include <Vex/Bindings.h>
#include <Vex/Buffer.h>
#include <Vex/RHIBindings.h>
#include <Vex/Texture.h>

namespace vex
{
class GfxBackend;

// This represents the different resources that will be used in the specific draw call/dispatch
class ResourceBindingSet
{
public:
    static void CollectRHIResources(GfxBackend& backend,
                                    std::span<const ResourceBinding> resources,
                                    std::vector<RHITextureBinding>& textureBindings,
                                    std::vector<RHIBufferBinding>& bufferBindings,
                                    TextureUsage::Type textureUsage,
                                    BufferUsage::Type bufferUsage);

    std::vector<ResourceBinding> reads;
    std::vector<ResourceBinding> writes;
    std::vector<ConstantBinding> constants;

    std::span<const ConstantBinding> GetConstantBindings() const noexcept
    {
        return constants;
    }

    void ValidateBindings() const;
};

} // namespace vex
