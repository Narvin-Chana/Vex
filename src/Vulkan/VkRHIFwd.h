#pragma once

namespace vex::vk
{

class VkCommandList;
class VkCommandPool;
class VkDescriptorPool;
class VkGraphicsPipelineState;
class VkComputePipelineState;
class VkSwapChain;
class VkResourceLayout;
class VkFence;
class VkTexture;
class VkBuffer;
class VkRHI;

} // namespace vex::vk

namespace vex
{

using RHICommandList = vk::VkCommandList;
using RHICommandPool = vk::VkCommandPool;
using RHIDescriptorPool = vk::VkDescriptorPool;
using RHIGraphicsPipelineState = vk::VkGraphicsPipelineState;
using RHIComputePipelineState = vk::VkComputePipelineState;
using RHISwapChain = vk::VkSwapChain;
using RHIResourceLayout = vk::VkResourceLayout;
using RHIFence = vk::VkFence;
using RHITexture = vk::VkTexture;
using RHIBuffer = vk::VkBuffer;
using RHI = vk::VkRHI;

} // namespace vex