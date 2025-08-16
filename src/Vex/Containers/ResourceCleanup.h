#pragma once

#include <variant>
#include <vector>

#include <Vex/MaybeUninitialized.h>
#include <Vex/RHIFwd.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class ResourceCleanup
{
public:
    using CleanupVariant = std::variant<MaybeUninitialized<RHITexture>,
                                        MaybeUninitialized<RHIBuffer>,
                                        UniqueHandle<RHIGraphicsPipelineState>,
                                        UniqueHandle<RHIComputePipelineState>,
                                        UniqueHandle<RHIRayTracingPipelineState>>;
    ResourceCleanup(i8 bufferingCount);

    void CleanupResource(CleanupVariant&& resource);
    void CleanupResource(CleanupVariant&& resource, i8 lifespan);
    void FlushResources(i8 flushCount, RHIDescriptorPool& descriptorPool, RHIAllocator& allocator);

private:
    i8 defaultLifespan;
    std::vector<std::pair<CleanupVariant, i8>> resourcesInFlight;
};

} // namespace vex