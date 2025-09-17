#pragma once

#include <RHI/RHIBarrier.h>

#include <Vulkan/VkHeaders.h>

namespace vex::vk
{

::vk::PipelineStageFlags2 RHIBarrierSyncToVulkan(RHIBarrierSync barrierSync);
::vk::AccessFlags2 RHIBarrierAccessToVulkan(RHIBarrierAccess barrierAccess);
::vk::ImageLayout RHITextureLayoutToVulkan(RHITextureLayout textureLayout);

} // namespace vex::vk