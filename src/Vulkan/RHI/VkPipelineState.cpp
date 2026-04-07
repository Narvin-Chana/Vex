#include "VkPipelineState.h"

#include <algorithm>

#include <Vex/PhysicalDevice.h>
#include <Vex/ResourceCleanup.h>
#include <Vex/Utility/ByteUtils.h>

#include <Vulkan/RHI/VkAccelerationStructure.h>
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkDebug.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
// These are necessary for ResourceCleanup
#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkGraphicsPipeline.h>

namespace vex::vk
{
VkGraphicsPipelineState::VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache psoCache)
    : RHIGraphicsPipelineStateBase(key)
    , device{ device }
    , psoCache{ psoCache }
{
    GraphicsPiplineUtils::ValidateGraphicsPipeline(key);
}

void VkGraphicsPipelineState::Compile(const Shader& vertexShader,
                                      const Shader& pixelShader,
                                      RHIResourceLayout& resourceLayout)
{
    Span<const byte> vsCode = vertexShader.GetBlob();
    ::vk::ShaderModuleCreateInfo vsShaderModuleCreateInfo{
        .codeSize = vsCode.size(),
        .pCode = reinterpret_cast<const u32*>(&vsCode[0]),
    };

    Span<const byte> psCode = pixelShader.GetBlob();
    ::vk::ShaderModuleCreateInfo psShaderModuleCreateInfo{
        .codeSize = psCode.size(),
        .pCode = reinterpret_cast<const u32*>(&psCode[0]),
    };

    auto vsShaderModule = VEX_VK_CHECK <<= device.createShaderModuleUnique(vsShaderModuleCreateInfo);
    auto psShaderModule = VEX_VK_CHECK <<= device.createShaderModuleUnique(psShaderModuleCreateInfo);

    std::array stages{ ::vk::PipelineShaderStageCreateInfo{ .stage = ::vk::ShaderStageFlagBits::eVertex,
                                                            .module = *vsShaderModule,
                                                            .pName = key.vertexShader.entryPoint.c_str() },
                       ::vk::PipelineShaderStageCreateInfo{ .stage = ::vk::ShaderStageFlagBits::eFragment,
                                                            .module = *psShaderModule,
                                                            .pName = key.pixelShader.entryPoint.c_str() } };

    std::vector<::vk::VertexInputBindingDescription> bindings{ key.vertexInputLayout.bindings.size() };
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
    for (u32 i = 0; i < attributes.size(); ++i)
    {
        const VertexInputLayout::VertexAttribute& attribute = key.vertexInputLayout.attributes[i];
        attributes[i] = ::vk::VertexInputAttributeDescription{ .location = i,
                                                               .binding = attribute.binding,
                                                               .format = TextureFormatToVulkan(attribute.format, false),
                                                               .offset = attribute.offset };
    }

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
        .dynamicStateCount = static_cast<u32>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    std::vector<::vk::Format> attachmentFormats;
    attachmentFormats.reserve(key.renderTargetState.colorFormats.size());
    for (const auto& [format, isSRGB] : key.renderTargetState.colorFormats)
    {
        attachmentFormats.emplace_back(TextureFormatToVulkan(format, isSRGB));
    }

    const ::vk::PipelineRenderingCreateInfoKHR pipelineRenderingCI{
        .colorAttachmentCount = static_cast<u32>(attachmentFormats.size()),
        .pColorAttachmentFormats = attachmentFormats.data(),
        .depthAttachmentFormat = TextureFormatToVulkan(key.renderTargetState.depthStencilFormat, false),
        .stencilAttachmentFormat = FormatUtil::IsDepthAndStencilFormat(key.renderTargetState.depthStencilFormat)
                                       ? TextureFormatToVulkan(key.renderTargetState.depthStencilFormat, false)
                                       : ::vk::Format::eUndefined
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

    graphicsPipeline = VEX_VK_CHECK <<= device.createGraphicsPipelineUnique(psoCache, graphicsPipelineCI);

    vertexShaderVersion = vertexShader.version;
    pixelShaderVersion = pixelShader.version;
    rootSignatureVersion = resourceLayout.version;

    SetDebugName(device, *graphicsPipeline, std::format("GraphicsPSO: {}", key).c_str());
}

std::unique_ptr<RHIGraphicsPipelineState> VkGraphicsPipelineState::Cleanup()
{
    if (!graphicsPipeline)
    {
        return nullptr;
    }
    auto cleanupPSO = std::make_unique<VkGraphicsPipelineState>(key, device, psoCache);
    std::swap(cleanupPSO->graphicsPipeline, graphicsPipeline);
    return cleanupPSO;
}

VkComputePipelineState::VkComputePipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache psoCache)
    : RHIComputePipelineStateBase(key)
    , device{ device }
    , psoCache{ psoCache }
{
}

void VkComputePipelineState::Compile(const Shader& computeShader, RHIResourceLayout& resourceLayout)
{
    Span<const byte> shaderCode = computeShader.GetBlob();
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
                .pName = key.computeShader.entryPoint.c_str(),
            },
        .layout = *resourceLayout.pipelineLayout,
    };

    computePipeline = VEX_VK_CHECK <<= device.createComputePipelineUnique(psoCache, computePipelineCreateInfo);

    computeShaderVersion = computeShader.version;
    rootSignatureVersion = resourceLayout.version;

    SetDebugName(device, *computePipeline, std::format("ComputePSO: {}", key).c_str());
}

std::unique_ptr<RHIComputePipelineState> VkComputePipelineState::Cleanup()
{
    if (!computePipeline)
    {
        return nullptr;
    }
    auto cleanupPSO = std::make_unique<VkComputePipelineState>(key, device, psoCache);
    std::swap(cleanupPSO->computePipeline, computePipeline);
    return cleanupPSO;
}

VkRayTracingPipelineState::VkRayTracingPipelineState(const Key& key,
                                                     NonNullPtr<VkGPUContext> ctx,
                                                     ::vk::PipelineCache psoCache)
    : RHIRayTracingPipelineStateBase(key)
    , ctx{ ctx }
    , psoCache{ psoCache }
{
}

std::vector<MaybeUninitialized<RHIBuffer>> VkRayTracingPipelineState::Compile(
    const RayTracingShaderCollection& shaderCollection, RHIResourceLayout& resourceLayout, RHIAllocator& allocator)
{
    auto CreateShaderModule = [&](const Shader& s)
    {
        Span<const byte> shaderCode = s.GetBlob();
        ::vk::ShaderModuleCreateInfo shaderModulecreateInfo{
            .codeSize = shaderCode.size(),
            .pCode = reinterpret_cast<const u32*>(&shaderCode[0]),
        };

        return VEX_VK_CHECK <<= ctx->device.createShaderModuleUnique(shaderModulecreateInfo);
    };

    std::vector<::vk::UniqueShaderModule> modules;
    std::vector<::vk::PipelineShaderStageCreateInfo> stages;
    std::vector<::vk::RayTracingShaderGroupCreateInfoKHR> groups;

    using VkShaderGroupCreateInfo = ::vk::RayTracingShaderGroupCreateInfoKHR;
    auto RegisterShaderStage = [&](const std::vector<NonNullPtr<Shader>>& shaders,
                                   ::vk::ShaderStageFlagBits type,
                                   ::vk::RayTracingShaderGroupTypeKHR groupType,
                                   uint32_t VkShaderGroupCreateInfo::* p)
    {
        for (u32 i = 0; i < shaders.size(); ++i)
        {
            modules.push_back(CreateShaderModule(*shaders[i]));

            ::vk::RayTracingShaderGroupCreateInfoKHR group{
                .type = groupType,
            };
            group.*p = stages.size();
            groups.push_back(group);

            stages.push_back({ .stage = type, .module = *modules.back(), .pName = shaders[i]->key.entryPoint.c_str() });
        }
    };

    RegisterShaderStage(shaderCollection.rayGenerationShaders,
                        ::vk::ShaderStageFlagBits::eRaygenKHR,
                        ::vk::RayTracingShaderGroupTypeKHR::eGeneral,
                        &::vk::RayTracingShaderGroupCreateInfoKHR::generalShader);
    RegisterShaderStage(shaderCollection.rayCallableShaders,
                        ::vk::ShaderStageFlagBits::eCallableKHR,
                        ::vk::RayTracingShaderGroupTypeKHR::eGeneral,
                        &::vk::RayTracingShaderGroupCreateInfoKHR::generalShader);
    RegisterShaderStage(shaderCollection.rayMissShaders,
                        ::vk::ShaderStageFlagBits::eMissKHR,
                        ::vk::RayTracingShaderGroupTypeKHR::eGeneral,
                        &::vk::RayTracingShaderGroupCreateInfoKHR::generalShader);

    for (const auto& group : shaderCollection.hitGroupShaders)
    {
        auto groupType = ::vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;

        u32 closesHitIndex = ::vk::ShaderUnusedKHR, anyHitIndex = ::vk::ShaderUnusedKHR,
            intersectionIndex = ::vk::ShaderUnusedKHR;

        closesHitIndex = modules.size();
        modules.push_back(CreateShaderModule(*group.rayClosestHitShader));
        stages.push_back({ .stage = ::vk::ShaderStageFlagBits::eClosestHitKHR,
                           .module = *modules.back(),
                           .pName = group.rayClosestHitShader->key.entryPoint.c_str() });

        if (group.rayAnyHitShader)
        {
            anyHitIndex = modules.size();
            modules.push_back(CreateShaderModule(**group.rayAnyHitShader));
            stages.push_back({ .stage = ::vk::ShaderStageFlagBits::eAnyHitKHR,
                               .module = *modules.back(),
                               .pName = (*group.rayAnyHitShader)->key.entryPoint.c_str() });
        }

        if (group.rayIntersectionShader)
        {
            // the presence of an intersection shader requires procedural hit group for custom intersection logic
            groupType = ::vk::RayTracingShaderGroupTypeKHR::eProceduralHitGroup;
            intersectionIndex = modules.size();
            modules.push_back(CreateShaderModule(**group.rayIntersectionShader));
            stages.push_back({ .stage = ::vk::ShaderStageFlagBits::eIntersectionKHR,
                               .module = *modules.back(),
                               .pName = (*group.rayIntersectionShader)->key.entryPoint.c_str() });
        }

        groups.push_back(::vk::RayTracingShaderGroupCreateInfoKHR{ .type = groupType,
                                                                   .closestHitShader = closesHitIndex,
                                                                   .anyHitShader = anyHitIndex,
                                                                   .intersectionShader = intersectionIndex });
    }

    ::vk::RayTracingPipelineCreateInfoKHR rtPSOCI{
        .stageCount = static_cast<u32>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<u32>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = key.maxRecursionDepth,
        .layout = *resourceLayout.pipelineLayout,
    };
    rtPipeline = VEX_VK_CHECK <<= ctx->device.createRayTracingPipelineKHRUnique({}, psoCache, rtPSOCI);

    auto ASProperties =
        GPhysicalDevice->physicalDevice
            .getProperties2<::vk::PhysicalDeviceProperties2, ::vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>()
            .get<::vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    u32 handleSize = ASProperties.shaderGroupHandleSize;

    std::vector<std::byte> groupHandles;
    u32 totalHandlesByteSize = handleSize * groups.size();
    groupHandles.resize(totalHandlesByteSize);
    // Buffer containing all handles for groups in pipeline
    VEX_VK_CHECK << ctx->device.getRayTracingShaderGroupHandlesKHR(*rtPipeline,
                                                                   0,
                                                                   groups.size(),
                                                                   totalHandlesByteSize,
                                                                   groupHandles.data());

    // 0: raygen, 1: raymiss, 2: group (closest hit, any hit and intersect), 3: callable
    std::array<std::vector<void*>, 4> handlesPerShaderType;
    for (int i = 0; i < stages.size(); ++i)
    {
        void* handlePtr = groupHandles.data() + i * handleSize;
        switch (stages[i].stage)
        {
        case ::vk::ShaderStageFlagBits::eRaygenKHR:
            handlesPerShaderType[0].push_back(handlePtr);
            break;
        case ::vk::ShaderStageFlagBits::eMissKHR:
            handlesPerShaderType[1].push_back(handlePtr);
            break;
        case ::vk::ShaderStageFlagBits::eClosestHitKHR:
        case ::vk::ShaderStageFlagBits::eAnyHitKHR:
        case ::vk::ShaderStageFlagBits::eIntersectionKHR:
            handlesPerShaderType[2].push_back(handlePtr);
            break;
        case ::vk::ShaderStageFlagBits::eCallableKHR:
            handlesPerShaderType[3].push_back(handlePtr);
            break;
        default:
            VEX_ASSERT(false, "This should never be reached");
        }
    }

    std::vector<MaybeUninitialized<RHIBuffer>> oldBuffers;
    if (rayGenTable)
    {
        oldBuffers.push_back(std::move(rayGenTable->shaderTableBuffer));
    }
    if (rayMissTable)
    {
        oldBuffers.push_back(std::move(rayMissTable->shaderTableBuffer));
    }
    if (groupHitTable)
    {
        oldBuffers.push_back(std::move(groupHitTable->shaderTableBuffer));
    }
    if (rayCallableTable)
    {
        oldBuffers.push_back(std::move(rayCallableTable->shaderTableBuffer));
    }

    rayGenTable = VkShaderTable(ctx, allocator, "Ray Gen shader Table", handlesPerShaderType[0]);

    if (!handlesPerShaderType[1].empty())
    {
        rayMissTable = VkShaderTable(ctx, allocator, "Ray Miss shader Table", handlesPerShaderType[1]);
    }

    if (!handlesPerShaderType[2].empty())
    {
        groupHitTable = VkShaderTable(ctx, allocator, "Group Hit shader Table", handlesPerShaderType[2]);
    }

    if (!handlesPerShaderType[3].empty())
    {
        rayCallableTable = VkShaderTable(ctx, allocator, "Ray Callable shader Table", handlesPerShaderType[3]);
    }

    return oldBuffers;
}

std::unique_ptr<RHIRayTracingPipelineState> VkRayTracingPipelineState::Cleanup()
{
    if (!rtPipeline)
    {
        return nullptr;
    }
    auto cleanupPSO = std::make_unique<VkRayTracingPipelineState>(key, ctx, psoCache);
    std::swap(cleanupPSO->rtPipeline, rtPipeline);

    std::swap(cleanupPSO->groupHitTable, groupHitTable);
    std::swap(cleanupPSO->rayCallableTable, rayCallableTable);
    std::swap(cleanupPSO->rayGenTable, rayGenTable);
    std::swap(cleanupPSO->rayMissTable, rayMissTable);

    return cleanupPSO;
}

} // namespace vex::vk