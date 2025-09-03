#include "VkPipelineState.h"

#include <algorithm>

#include <Vex/Containers/ResourceCleanup.h>

#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
// These are necessary for ResourceCleanup
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkGraphicsPipeline.h>

namespace vex::vk
{

VkGraphicsPipelineState::VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIGraphicsPipelineStateBase(key)
    , device{ device }
    , PSOCache{ PSOCache }
{
    GraphicsPiplineUtils::ValidateGraphicsPipeline(key);
}

void VkGraphicsPipelineState::Compile(const Shader& vertexShader,
                                      const Shader& pixelShader,
                                      RHIResourceLayout& resourceLayout)
{
    std::span<const u8> vsCode = vertexShader.GetBlob();
    ::vk::ShaderModuleCreateInfo vsShaderModuleCreateInfo{
        .codeSize = vsCode.size(),
        .pCode = reinterpret_cast<const u32*>(&vsCode[0]),
    };

    std::span<const u8> psCode = pixelShader.GetBlob();
    ::vk::ShaderModuleCreateInfo psShaderModuleCreateInfo{
        .codeSize = psCode.size(),
        .pCode = reinterpret_cast<const u32*>(&psCode[0]),
    };

    auto vsShaderModule = VEX_VK_CHECK <<= device.createShaderModuleUnique(vsShaderModuleCreateInfo);
    auto psShaderModule = VEX_VK_CHECK <<= device.createShaderModuleUnique(psShaderModuleCreateInfo);

    std::array stages{ ::vk::PipelineShaderStageCreateInfo{ .stage = ::vk::ShaderStageFlagBits::eVertex,
                                                            .module = *vsShaderModule,
                                                            .pName = "VSMain" },
                       ::vk::PipelineShaderStageCreateInfo{ .stage = ::vk::ShaderStageFlagBits::eFragment,
                                                            .module = *psShaderModule,
                                                            .pName = "PSMain" } };

    std::vector<::vk::VertexInputBindingDescription> bindings{ key.vertexInputLayout.bindings.size() };
    // Requires including the heavy <algorithm>
    std::ranges::transform(key.vertexInputLayout.bindings,
                           bindings.begin(),
                           [](const VertexInputLayout::VertexBinding& binding)
                           {
                               return ::vk::VertexInputBindingDescription{
                                   .binding = binding.binding,
                                   .stride = binding.strideByteSize,
                                   .inputRate = GraphicsPiplineUtils::InputRateToVkInputRate(binding.inputRate)
                               };
                           });

    std::vector<::vk::VertexInputAttributeDescription> attributes{ key.vertexInputLayout.attributes.size() };
    // Requires including the heavy <algorithm>
    std::ranges::transform(
        key.vertexInputLayout.attributes,
        attributes.begin(),
        [](const VertexInputLayout::VertexAttribute& attribute)
        {
            return ::vk::VertexInputAttributeDescription{ .location = attribute.semanticIndex,
                                                          .binding = attribute.binding,
                                                          .format = TextureFormatToVulkan(attribute.format),
                                                          .offset = attribute.offset };
        });

    ::vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCI{
        .vertexBindingDescriptionCount = static_cast<u32>(bindings.size()),
        .pVertexBindingDescriptions = bindings.data(),
        .vertexAttributeDescriptionCount = static_cast<u32>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data()
    };

    ::vk::PipelineInputAssemblyStateCreateInfo inputAssemblyState{
        .topology = GraphicsPiplineUtils::InputTopologyToVkTopology(key.inputAssembly.topology),
        .primitiveRestartEnable = key.inputAssembly.primitiveRestartEnabled,
    };

    ::vk::PipelineRasterizationStateCreateInfo rasterCI{
        .depthClampEnable = key.rasterizerState.depthClampEnabled,
        .rasterizerDiscardEnable = key.rasterizerState.rasterizerDiscardEnabled,
        .polygonMode = GraphicsPiplineUtils::PolygonModeToVkPolygonMode(key.rasterizerState.polygonMode),
        .cullMode = GraphicsPiplineUtils::CullModeToVkCullMode(key.rasterizerState.cullMode),
        .frontFace = GraphicsPiplineUtils::WindingToVkFrontFace(key.rasterizerState.winding),
        .depthBiasEnable = key.rasterizerState.depthBiasEnabled,
        .depthBiasConstantFactor = key.rasterizerState.depthBiasConstantFactor,
        .depthBiasClamp = key.rasterizerState.depthBiasClamp,
        .depthBiasSlopeFactor = key.rasterizerState.depthBiasSlopeFactor,
        .lineWidth = key.rasterizerState.lineWidth,
    };

    ::vk::PipelineMultisampleStateCreateInfo multisamplingStateCI{
        .rasterizationSamples = ::vk::SampleCountFlagBits::e1,
        .sampleShadingEnable = false,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = false,
        .alphaToOneEnable = false,
    };

    ::vk::PipelineDepthStencilStateCreateInfo depthStateCI{
        .depthTestEnable = key.depthStencilState.depthTestEnabled,
        .depthWriteEnable = key.depthStencilState.depthWriteEnabled,
        .depthCompareOp = static_cast<::vk::CompareOp>(key.depthStencilState.depthCompareOp),
        .depthBoundsTestEnable = key.depthStencilState.depthBoundsTestEnabled,
        .stencilTestEnable = key.depthStencilState.stencilTestEnabled,
        .front = GraphicsPiplineUtils::StencilOpStateToVkStencilOpState(key.depthStencilState.front),
        .back = GraphicsPiplineUtils::StencilOpStateToVkStencilOpState(key.depthStencilState.back),
        .minDepthBounds = key.depthStencilState.minDepthBounds,
        .maxDepthBounds = key.depthStencilState.maxDepthBounds,
    };

    std::vector<::vk::PipelineColorBlendAttachmentState> colorAttachments(key.colorBlendState.attachments.size());
    // Requires including the heavy <algorithm>
    std::ranges::transform(
        key.colorBlendState.attachments,
        colorAttachments.begin(),
        [](const ColorBlendState::ColorBlendAttachment& attachment)
        {
            return ::vk::PipelineColorBlendAttachmentState{
                .blendEnable = attachment.blendEnabled,
                .srcColorBlendFactor = static_cast<::vk::BlendFactor>(attachment.srcColorBlendFactor),
                .dstColorBlendFactor = static_cast<::vk::BlendFactor>(attachment.dstColorBlendFactor),
                .colorBlendOp = static_cast<::vk::BlendOp>(attachment.colorBlendOp),
                .srcAlphaBlendFactor = static_cast<::vk::BlendFactor>(attachment.srcAlphaBlendFactor),
                .dstAlphaBlendFactor = static_cast<::vk::BlendFactor>(attachment.dstAlphaBlendFactor),
                .alphaBlendOp = static_cast<::vk::BlendOp>(attachment.alphaBlendOp),
                .colorWriteMask = static_cast<::vk::ColorComponentFlags>(attachment.colorWriteMask)
            };
        });

    ::vk::PipelineColorBlendStateCreateInfo blendStateCI{
        .logicOpEnable = key.colorBlendState.logicOpEnabled,
        .logicOp = static_cast<::vk::LogicOp>(key.colorBlendState.logicOp),
        .attachmentCount = static_cast<u32>(colorAttachments.size()),
        .pAttachments = colorAttachments.data(),
        .blendConstants = key.colorBlendState.blendConstants,
    };

    std::array dynamicStates = { ::vk::DynamicState::eViewportWithCount,
                                 ::vk::DynamicState::eScissorWithCount,
                                 ::vk::DynamicState::ePrimitiveTopology,
                                 ::vk::DynamicState::ePrimitiveRestartEnable };

    ::vk::PipelineDynamicStateCreateInfo dynamicStateInfo{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    std::vector<::vk::Format> attachmentFormats(key.renderTargetState.colorFormats.size());
    // Requires including the heavy <algorithm>
    std::ranges::transform(key.renderTargetState.colorFormats, attachmentFormats.begin(), TextureFormatToVulkan);

    const ::vk::PipelineRenderingCreateInfoKHR pipelineRenderingCI{
        .colorAttachmentCount = static_cast<u32>(attachmentFormats.size()),
        .pColorAttachmentFormats = attachmentFormats.data(),
        .depthAttachmentFormat = TextureFormatToVulkan(key.depthStencilState.depthStencilFormat),
        .stencilAttachmentFormat =
            TextureFormatToVulkan(DoesFormatSupportStencil(key.depthStencilState.depthStencilFormat)
                                      ? key.depthStencilState.depthStencilFormat
                                      : TextureFormat::UNKNOWN)
    };

    ::vk::PipelineViewportStateCreateInfo viewportStateCI{
        .viewportCount = 0,
        .pViewports = nullptr,
        .scissorCount = 0,
        .pScissors = nullptr,
    };

    ::vk::GraphicsPipelineCreateInfo graphicsPipelineCI{ .pNext = &pipelineRenderingCI,
                                                         .stageCount = stages.size(),
                                                         .pStages = stages.data(),
                                                         .pVertexInputState = &pipelineVertexInputStateCI,
                                                         .pInputAssemblyState = &inputAssemblyState,
                                                         .pViewportState = &viewportStateCI,
                                                         .pRasterizationState = &rasterCI,
                                                         .pMultisampleState = &multisamplingStateCI,
                                                         .pDepthStencilState = &depthStateCI,
                                                         .pColorBlendState = &blendStateCI,
                                                         .pDynamicState = &dynamicStateInfo,
                                                         .layout = *resourceLayout.pipelineLayout,
                                                         .renderPass = nullptr,
                                                         .subpass = 0,
                                                         .basePipelineHandle = nullptr,
                                                         .basePipelineIndex = -1 };

    graphicsPipeline = VEX_VK_CHECK <<= device.createGraphicsPipelineUnique(PSOCache, graphicsPipelineCI);
}

void VkGraphicsPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    if (!graphicsPipeline)
    {
        return;
    }
    auto cleanupPSO = MakeUnique<VkGraphicsPipelineState>(key, device, PSOCache);
    std::swap(cleanupPSO->graphicsPipeline, graphicsPipeline);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

VkComputePipelineState::VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIComputePipelineStateInterface(key)
    , device{ device }
    , PSOCache{ PSOCache }
{
}

void VkComputePipelineState::Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout)
{
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
        .layout = *resourceLayout.pipelineLayout,
    };

    computePipeline = VEX_VK_CHECK <<= device.createComputePipelineUnique(PSOCache, computePipelineCreateInfo);
}

void VkComputePipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    if (!computePipeline)
    {
        return;
    }
    auto cleanupPSO = MakeUnique<VkComputePipelineState>(key, device, PSOCache);
    std::swap(cleanupPSO->computePipeline, computePipeline);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

VkRayTracingPipelineState::VkRayTracingPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIRayTracingPipelineStateInterface(key)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkRayTracingPipelineState::Compile(const RayTracingShaderCollection& shaderCollection,
                                        RHIResourceLayout& resourceLayout,
                                        ResourceCleanup& resourceCleanup,
                                        RHIAllocator& allocator)
{
    VEX_NOT_YET_IMPLEMENTED();
}

void VkRayTracingPipelineState::Cleanup(ResourceCleanup& resourceCleanup)
{
    // Don't cleanup if RTPSO is null.
    if (!!1)
    {
        return;
    }
    VEX_NOT_YET_IMPLEMENTED();
}

} // namespace vex::vk