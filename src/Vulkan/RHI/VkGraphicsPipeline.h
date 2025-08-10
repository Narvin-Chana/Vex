#pragma once

#include <Vex/GraphicsPipeline.h>

#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkMacros.h>

namespace vex
{
struct GraphicsPipelineStateKey;
}
namespace vex::vk
{
namespace GraphicsPiplineUtils
{
VEX_VK_DECLARE_ENUM_MAPPING(Winding, Winding, ::vk::FrontFace, VkFrontFace);
VEX_VK_DECLARE_ENUM_MAPPING(InputTopology, InputTopology, ::vk::PrimitiveTopology, VkTopology);
VEX_VK_DECLARE_ENUM_MAPPING(PolygonMode, PolygonMode, ::vk::PolygonMode, VkPolygonMode);
VEX_VK_DECLARE_ENUM_MAPPING(VertexInputLayout::InputRate, InputRate, ::vk::VertexInputRate, VkInputRate);
VEX_VK_DECLARE_ENUM_MAPPING_FLAGS(CullMode, CullMode, ::vk::CullMode, VkCullMode);

::vk::StencilOpState StencilOpStateToVkStencilOpState(DepthStencilState::StencilOpState op);
void ValidateGraphicsPipeline(const GraphicsPipelineStateKey& graphicsPipelineKey);
} // namespace GraphicsPiplineUtils

} // namespace vex::vk
