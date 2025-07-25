#pragma once

#include <vector>

#include <Vex/Bindings.h>
#include <Vex/RHI/RHIBindings.h>

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
                                    ResourceUsage::Type usage);

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
