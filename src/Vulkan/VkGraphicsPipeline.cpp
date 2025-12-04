#include "VkGraphicsPipeline.h"

#include <Vex/Containers/ResourceCleanup.h>
#include <Vex/Logger.h>
#include <Vex/Utility/UniqueHandle.h>

#include <Vulkan/RHI/VkBuffer.h>
#include <Vulkan/RHI/VkPipelineState.h>
#include <Vulkan/RHI/VkResourceLayout.h>
#include <Vulkan/VkErrorHandler.h>
#include <Vulkan/VkFormats.h>
#include <Vulkan/VkMacros.h>

namespace vex::vk
{

namespace GraphicsPiplineUtils
{

// clang-format off

VEX_VK_BEGIN_ENUM_MAPPING(InputTopology, InputTopology, ::vk::PrimitiveTopology, VkTopology)
    VEX_VK_ENUM_MAPPING_ENTRY(TriangleFan, eTriangleFan)
    VEX_VK_ENUM_MAPPING_ENTRY(TriangleList, eTriangleList)
    VEX_VK_ENUM_MAPPING_ENTRY(TriangleStrip, eTriangleStrip)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(VertexInputLayout::InputRate, InputRate, ::vk::VertexInputRate, VkInputRate)
    VEX_VK_ENUM_MAPPING_ENTRY(PerInstance, eInstance)
    VEX_VK_ENUM_MAPPING_ENTRY(PerVertex, eVertex)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING_FLAGS(CullMode, CullMode, ::vk::CullMode, VkCullMode)
    VEX_VK_ENUM_MAPPING_ENTRY(Back, eBack)
    VEX_VK_ENUM_MAPPING_ENTRY(Front, eFront)
    VEX_VK_ENUM_MAPPING_ENTRY(None, eNone)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(Winding, Winding, ::vk::FrontFace, VkFrontFace)
    VEX_VK_ENUM_MAPPING_ENTRY(Clockwise, eClockwise)
    VEX_VK_ENUM_MAPPING_ENTRY(CounterClockwise, eCounterClockwise)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(PolygonMode, PolygonMode, ::vk::PolygonMode, VkPolygonMode)
    VEX_VK_ENUM_MAPPING_ENTRY(Fill, eFill)
    VEX_VK_ENUM_MAPPING_ENTRY(Line, eLine)
    VEX_VK_ENUM_MAPPING_ENTRY(Point, ePoint)
VEX_VK_END_ENUM_MAPPING

// clang-format on

::vk::StencilOpState StencilOpStateToVkStencilOpState(DepthStencilState::StencilOpState op)
{
    const auto& [failOp, passOp, depthFailOp, compareOp, readMask, writeMask, ref] = op;
    return ::vk::StencilOpState{
        .failOp = static_cast<::vk::StencilOp>(failOp),
        .passOp = static_cast<::vk::StencilOp>(passOp),
        .depthFailOp = static_cast<::vk::StencilOp>(depthFailOp),
        .compareOp = static_cast<::vk::CompareOp>(compareOp),
        .compareMask = readMask,
        .writeMask = writeMask,
        .reference = ref,
    };
}

void ValidateGraphicsPipeline(const GraphicsPipelineStateKey& key)
{
    // TODO: add validation nothing obvious comes to mind for the moment
}

} // namespace GraphicsPiplineUtils

} // namespace vex::vk