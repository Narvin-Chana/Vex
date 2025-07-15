#pragma once
#include "Bindings.h"
#include "RHI/RHIBindings.h"

#include <vector>

namespace vex
{
class GfxBackend;

// This represents the different resources that will be used in the specific draw call/dispatch
class ResourceBindingSet
{
    std::vector<ResourceBinding> reads;
    std::vector<ResourceBinding> writes;
    std::vector<ConstantBinding> constants;

    static void CollectResources(GfxBackend& backend,
                                 std::span<const ResourceBinding> resources,
                                 std::vector<RHITextureBinding>& textureBindings,
                                 std::vector<RHIBufferBinding>& bufferBindings,
                                 ResourceUsage::Type usage);

public:
    ResourceBindingSet();
    ResourceBindingSet& SetWriteBindings(std::span<const ResourceBinding> bindings);
    ResourceBindingSet& SetReadBindings(std::span<const ResourceBinding> bindings);
    ResourceBindingSet& SetConstantsBindings(std::span<const ConstantBinding> bindings);

    std::span<const ConstantBinding> GetConstantBindings() const noexcept
    {
        return constants;
    }

    void ValidateBindings() const;

    void CollectWriteBindings(GfxBackend& backend,
                              std::vector<RHITextureBinding>& textureBindings,
                              std::vector<RHIBufferBinding>& bufferBindings) const;
    void CollectReadBindings(GfxBackend& backend,
                             std::vector<RHITextureBinding>& textureBindings,
                             std::vector<RHIBufferBinding>& bufferBindings) const;
};

} // namespace vex
