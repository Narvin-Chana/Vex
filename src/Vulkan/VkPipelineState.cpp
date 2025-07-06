#include "VkPipelineState.h"

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Debug.h>

namespace vex::vk
{

VkGraphicsPipelineState::VkGraphicsPipelineState(const Key& key)
    : RHIGraphicsPipelineState(key)
{
}

void VkGraphicsPipelineState::Compile(const RHIShader& vertexShader,
                                      const RHIShader& pixelShader,
                                      RHIResourceLayout& resourceLayout)
{
    VEX_NOT_YET_IMPLEMENTED();
}

VkComputePipelineState::VkComputePipelineState(const Key& key)
    : RHIComputePipelineState(key)
{
}

void VkComputePipelineState::Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkComputePipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex::vk