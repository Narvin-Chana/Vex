#include "VkPipelineState.h"
#include "Vex/RHI/RHIShader.h"
#include "VkErrorHandler.h"
#include "VkResourceLayout.h"

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Debug.h>
#include <Vex/RHI/RHIBuffer.h>
#include <Vex/RHI/RHIShader.h>
#include <Vex/RHI/RHITexture.h>

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

bool VkGraphicsPipelineState::NeedsRecompile(const Key& newKey)
{
    // TODO: determine what is needed and what isn't, dynamic rendering should reduce how we compare this.
    VEX_NOT_YET_IMPLEMENTED();
    return false;
}

void VkGraphicsPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    VEX_NOT_YET_IMPLEMENTED();
}

VkComputePipelineState::VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIComputePipelineState(key)
    , device{ device }
    , PSOCache{ PSOCache }
{
}

void VkComputePipelineState::Compile(const RHIShader& computeShader, RHIResourceLayout& resourceLayout)
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