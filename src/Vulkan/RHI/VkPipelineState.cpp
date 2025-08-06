#include "VkPipelineState.h"

#include <ranges>

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Debug.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/RHI/VkTexture.h>
#include <Vulkan/VkErrorHandler.h>

#include "Vulkan/VkFormats.h"

namespace vex::vk
{

namespace GraphicsPiplineUtils
{

::vk::PrimitiveTopology InputTopologyToVkTopology(InputTopology topo)
{
    using enum ::vk::PrimitiveTopology;
    switch (topo)
    {
    case InputTopology::TriangleFan:
        return eTriangleFan;
    case InputTopology::TriangleList:
        return eTriangleList;
    case InputTopology::TriangleStrip:
        return eTriangleStrip;
    default:
        VEX_LOG(Fatal, "Unsupported InputTopology. Add them to the switch?");
        break;
    }
    std::unreachable();
}

static ::vk::VertexInputRate InputRateToVkInputRate(VertexInputLayout::InputRate inputRate)
{
    switch (inputRate)
    {
    case VertexInputLayout::PerInstance:
        return ::vk::VertexInputRate::eInstance;
    case VertexInputLayout::PerVertex:
        return ::vk::VertexInputRate::eVertex;
    default:
        VEX_LOG(Fatal, "VertexInputLayout not supported");
    }
    std::unreachable();
}

static ::vk::CullModeFlags CullModeToVkCullMode(CullMode cullMode)
{
    switch (cullMode)
    {
    case CullMode::Back:
        return ::vk::CullModeFlagBits::eBack;
    case CullMode::Front:
        return ::vk::CullModeFlagBits::eFront;
    case CullMode::None:
        return ::vk::CullModeFlagBits::eNone;
    default:
        VEX_LOG(Fatal, "CullMode not supported");
    }
    std::unreachable();
}

static ::vk::FrontFace WindingToVkFrontFace(Winding winding)
{
    switch (winding)
    {
    case Winding::Clockwise:
        return ::vk::FrontFace::eClockwise;
    case Winding::CounterClockwise:
        return ::vk::FrontFace::eCounterClockwise;
    default:
        VEX_LOG(Fatal, "Winding not supported");
    }
    std::unreachable();
}
static ::vk::PolygonMode PolygonModeToVkPolygonMode(PolygonMode winding)
{
    using enum ::vk::PolygonMode;
    switch (winding)
    {
    case PolygonMode::Fill:
        return eFill;
    case PolygonMode::Line:
        return eLine;
    case PolygonMode::Point:
        return ePoint;
    default:
        VEX_LOG(Fatal, "PolygonMode not supported");
    }
    std::unreachable();
}

} // namespace GraphicsPiplineUtils

namespace GraphicsPipeline_Internal
{

}

VkGraphicsPipelineState::VkGraphicsPipelineState(const Key& key, ::vk::Device device, ::vk::PipelineCache PSOCache)
    : RHIGraphicsPipelineStateInterface(key)
    , device{ device }
    , PSOCache{ PSOCache }
{
}

VkGraphicsPipelineState::~VkGraphicsPipelineState() = default;

void VkGraphicsPipelineState::Compile(const Shader& vertexShader,
                                      const Shader& pixelShader,
                                      RHIResourceLayout& resourceLayout)
{
    std::span<const u8> vsCode = vertexShader.GetBlob();
    ::vk::ShaderModuleCreateInfo vsShaderModuleCreateInfo{
        .codeSize = vsCode.size(),
        .pCode = reinterpret_cast<const u32*>(&vsCode[0]),
    };

    std::span<const u8> psCode = vertexShader.GetBlob();
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
    std::ranges::transform(key.vertexInputLayout.bindings,
                           bindings.begin(),
                           [](const VertexInputLayout::VertexBinding& binding)
                           {
                               return ::vk::VertexInputBindingDescription{
                                   .binding = binding.binding,
                                   .stride = binding.stride,
                                   .inputRate = GraphicsPiplineUtils::InputRateToVkInputRate(binding.inputRate)
                               };
                           });

    std::vector<::vk::VertexInputAttributeDescription> attributes{ key.vertexInputLayout.attributes.size() };
    std::ranges::transform(
        key.vertexInputLayout.attributes,
        attributes.begin(),
        [](const VertexInputLayout::VertexAttribute& attribute)
        {
            return ::vk::VertexInputAttributeDescription{ .binding = attribute.binding,
                                                          .format = TextureFormatToVulkan(attribute.format),
                                                          .location = attribute.semanticIndex,
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
        .flags = {}
    };

    ::vk::PipelineRasterizationStateCreateInfo rasterCI{
        .cullMode = GraphicsPiplineUtils::CullModeToVkCullMode(key.rasterizerState.cullMode),
        .depthBiasClamp = key.rasterizerState.depthBiasClamp,
        .depthBiasConstantFactor = key.rasterizerState.depthBiasConstantFactor,
        .depthClampEnable = key.rasterizerState.depthClampEnabled,
        .depthBiasEnable = key.rasterizerState.depthBiasEnabled,
        .depthBiasSlopeFactor = key.rasterizerState.depthBiasSlopeFactor,
        .frontFace = GraphicsPiplineUtils::WindingToVkFrontFace(key.rasterizerState.winding),
        .lineWidth = key.rasterizerState.lineWidth,
        .polygonMode = GraphicsPiplineUtils::PolygonModeToVkPolygonMode(key.rasterizerState.polygonMode),
        .rasterizerDiscardEnable = key.rasterizerState.rasterizerDiscardEnabled
    };

    ::vk::GraphicsPipelineCreateInfo graphicsPipelineCI{ .pStages = stages.data(),
                                                         .stageCount = stages.size(),
                                                         .pVertexInputState = &pipelineVertexInputStateCI,
                                                         .pInputAssemblyState = &inputAssemblyState,
                                                         .pViewportState = nullptr, // TODO ?
                                                         .pRasterizationState = &rasterCI,
                                                         .pMultisampleState = nullptr,  // TODO
                                                         .pDepthStencilState = nullptr, // TODO
                                                         .pColorBlendState = nullptr,   // TODO
                                                         .pDynamicState = nullptr,      // TODO
                                                         .layout = *resourceLayout.pipelineLayout,
                                                         .renderPass = nullptr, // TODO ?
                                                         .subpass = 0,
                                                         .basePipelineHandle = nullptr,
                                                         .basePipelineIndex = -1 };

    graphicsPipeline = VEX_VK_CHECK <<= device.createGraphicsPipelineUnique(PSOCache, graphicsPipelineCI);
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
    auto cleanupPSO = MakeUnique<VkComputePipelineState>(key, device, PSOCache);
    std::swap(cleanupPSO->computePipeline, computePipeline);
    resourceCleanup.CleanupResource(std::move(cleanupPSO));
}

} // namespace vex::vk