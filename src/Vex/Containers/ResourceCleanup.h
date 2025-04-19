#pragma once

#include <variant>
#include <vector>

#include <Vex/RHI/RHIFwd.h>
#include <Vex/Types.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class ResourceCleanup
{
public:
    using CleanupVariant = std::variant<UniqueHandle<RHITexture>,
                                        UniqueHandle<RHIBuffer>,
                                        UniqueHandle<RHIGraphicsPipelineState>,
                                        UniqueHandle<RHIComputePipelineState>>;
    ResourceCleanup(i8 bufferingCount);

    void CleanupResource(CleanupVariant resource);
    void CleanupResource(CleanupVariant resource, i8 lifespan);
    void FlushResources(i8 flushCount);

private:
    i8 defaultLifespan;
    std::vector<std::pair<CleanupVariant, i8>> resourcesInFlight;
};

} // namespace vex