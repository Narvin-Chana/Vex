#pragma once

#include <memory>
#include <variant>

#include <Vex/Utility/MaybeUninitialized.h>
#include <Vex/RHIImpl/RHIAccelerationStructure.h>
#include <Vex/RHIImpl/RHIBuffer.h>
#include <Vex/RHIImpl/RHIPipelineState.h>
#include <Vex/RHIImpl/RHITexture.h>

#include <RHI/RHIFwd.h>

namespace vex
{

using CleanupVariant = std::variant<std::unique_ptr<RHITexture>,
                                    std::unique_ptr<RHIBuffer>,
                                    // Required for internal RHI allocation cleanup.
                                    MaybeUninitialized<RHIBuffer>,
                                    std::unique_ptr<RHIAccelerationStructure>,
                                    std::unique_ptr<RHIGraphicsPipelineState>,
                                    std::unique_ptr<RHIComputePipelineState>,
                                    std::unique_ptr<RHIRayTracingPipelineState>>;

void CleanupResource(CleanupVariant&& resource, RHIDescriptorPool& descriptorPool, RHIAllocator& allocator);

} // namespace vex