#include "VkPipelineState.h"

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Debug.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkErrorHandler.h>

namespace vex::vk
{

VkGraphicsPipelineState::VkGraphicsPipelineState(const Key& key)
    : RHIGraphicsPipelineStateInterface(key)
{
}

VkGraphicsPipelineState::~VkGraphicsPipelineState() = default;

void VkGraphicsPipelineState::Compile(const Shader& vertexShader,
                                      const Shader& pixelShader,
                                      RHIResourceLayout& resourceLayout)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkGraphicsPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    VEX_NOT_YET_IMPLEMENTED();
}

VkComputePipelineState::VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIComputePipelineStateInterface(key)
    , device{ device }
    , PSOCache{ PSOCache }
{
}

VkComputePipelineState::~VkComputePipelineState() = default;

void VkComputePipelineState::Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout)
{
    auto& vkResourceLayout = reinterpret_cast<VkResourceLayout&>(resourceLayout);

    std::span<const u8> shaderCode = computeShader.GetBlob();
    ::vk::ShaderModuleCreateInfo shaderModulecreateInfo{
        .codeSize = shaderCode.size(),
        .pCode = reinterpret_cast<const u32*>(&shaderCode[0]),
    };

    auto computeShaderModule = VEX_VK_CHECK <<= device.createShaderModuleUnique(shaderModulecreateInfo);

    ::vk::ComputePipelineCreateInfo computePipelineCreateInfo{
        .stage =
            ::vk::PipelineShaderStageCreateInfo{
                .stage = ::vk::ShaderStageFlagBits::eCompute,
                .module = *computeShaderModule,
                .pName = "CSMain",
            },
        .layout = *vkResourceLayout.pipelineLayout,
    };

    computePipeline = VEX_VK_CHECK <<= device.createComputePipelineUnique(PSOCache, computePipelineCreateInfo);
}

void VkComputePipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    auto cleanupPSO = MakeUnique<VkComputePipelineState>(key, device, PSOCache);
    std::swap(cleanupPSO->computePipeline, computePipeline);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

} // namespace vex::vk