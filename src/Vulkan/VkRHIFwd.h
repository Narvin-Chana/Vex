#pragma once
#include <cstddef>

namespace vex::vk
{

class VkAllocator;
class VkCommandList;
class VkCommandPool;
class VkDescriptorPool;
class VkGraphicsPipelineState;
class VkComputePipelineState;
class VkRayTracingPipelineState;
class VkSwapChain;
class VkResourceLayout;
class VkTexture;
class VkBuffer;
class VkRHI;
class VkFence;
class VkDescriptorSet;
class VkBindlessDescriptorSet;

} // namespace vex::vk

namespace vex
{

// Vulkan limit is a max of 128, but could be potentially less depending on the device.
static constexpr std::size_t MaxTheoreticalLocalConstantsByteSize = 128;

using RHIAllocator = vk::VkAllocator;
using RHICommandList = vk::VkCommandList;
using RHICommandPool = vk::VkCommandPool;
using RHIDescriptorPool = vk::VkDescriptorPool;
using RHIDescriptorSet = vk::VkDescriptorSet;
using RHIBindlessDescriptorSet = vk::VkBindlessDescriptorSet;
using RHIGraphicsPipelineState = vk::VkGraphicsPipelineState;
using RHIComputePipelineState = vk::VkComputePipelineState;
using RHIRayTracingPipelineState = vk::VkRayTracingPipelineState;
using RHISwapChain = vk::VkSwapChain;
using RHIResourceLayout = vk::VkResourceLayout;
using RHITexture = vk::VkTexture;
using RHIBuffer = vk::VkBuffer;
using RHI = vk::VkRHI;
using RHIFence = vk::VkFence;

} // namespace vex