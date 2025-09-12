#pragma once

#include <Vex/Logger.h>
#include <Vex/TextureSampler.h>

#include <Vulkan/VkHeaders.h>
#include <Vulkan/VkMacros.h>

namespace vex::vk
{

static constexpr u32 MaxSamplerCount = 1024;

VEX_VK_BEGIN_ENUM_MAPPING(AddressMode, AddressMode, ::vk::SamplerAddressMode, VkSamplerAddressMode)
VEX_VK_ENUM_MAPPING_ENTRY(Wrap, eRepeat)
VEX_VK_ENUM_MAPPING_ENTRY(Border, eClampToBorder)
VEX_VK_ENUM_MAPPING_ENTRY(Mirror, eMirroredRepeat)
VEX_VK_ENUM_MAPPING_ENTRY(Clamp, eClampToEdge)
VEX_VK_ENUM_MAPPING_ENTRY(MirrorOnce, eMirrorClampToEdge)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(FilterMode, FilterMode, ::vk::Filter, VkFilter)
VEX_VK_ENUM_MAPPING_ENTRY(Linear, eLinear)
VEX_VK_ENUM_MAPPING_ENTRY(Point, eNearest)
VEX_VK_ENUM_MAPPING_ENTRY(Anisotropic, eLinear)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(FilterMode, FilterMode, ::vk::SamplerMipmapMode, VkMipMapMode)
VEX_VK_ENUM_MAPPING_ENTRY(Linear, eLinear)
VEX_VK_ENUM_MAPPING_ENTRY(Point, eNearest)
VEX_VK_ENUM_MAPPING_ENTRY(Anisotropic, eLinear)
VEX_VK_END_ENUM_MAPPING

VEX_VK_BEGIN_ENUM_MAPPING(BorderColor, BorderColor, ::vk::BorderColor, VkBorderColor)
VEX_VK_ENUM_MAPPING_ENTRY(OpaqueBlackFloat, eFloatOpaqueBlack)
VEX_VK_ENUM_MAPPING_ENTRY(OpaqueBlackInt, eIntOpaqueBlack)
VEX_VK_ENUM_MAPPING_ENTRY(OpaqueWhiteFloat, eFloatOpaqueWhite)
VEX_VK_ENUM_MAPPING_ENTRY(OpaqueWhiteInt, eIntOpaqueWhite)
VEX_VK_ENUM_MAPPING_ENTRY(TransparentBlackFloat, eFloatTransparentBlack)
VEX_VK_ENUM_MAPPING_ENTRY(TransparentBlackInt, eIntTransparentBlack)
VEX_VK_END_ENUM_MAPPING

} // namespace vex::vk