#pragma once

#include <utility>
#include <variant>
#include <vector>

#include <Vex/Utility/MaybeUninitialized.h>
#include <Vex/Utility/NonNullPtr.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHITexture.h>
#include <Vex/Synchronization.h>
#include <Vex/Utility/UniqueHandle.h>

#include <RHI/RHIFwd.h>

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

    ResourceCleanup(NonNullPtr<RHI> rhi);

    void CleanupResource(CleanupVariant&& resource);
    void FlushResources(RHIDescriptorPool& descriptorPool, RHIAllocator& allocator);

private:
    NonNullPtr<RHI> rhi;
    std::vector<std::pair<CleanupVariant, std::array<SyncToken, QueueTypes::Count>>> resourcesInFlight;
};

} // namespace vex