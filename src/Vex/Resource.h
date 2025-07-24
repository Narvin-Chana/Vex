#pragma once

#include <Vex/EnumFlags.h>
#include <Vex/Types.h>

namespace vex
{

// clang-format off

BEGIN_VEX_ENUM_FLAGS(ResourceUsage, u8)
    None            = 0,
    Read            = 1 << 0, // SRV in DX12, Sampled/Combined Image in Vulkan
    UnorderedAccess = 1 << 1, // UAV in DX12, Storage Image in Vulkan
    RenderTarget    = 1 << 2, // RTV in DX12, Color Attachment in Vulkan
    DepthStencil    = 1 << 3, // DSV in DX12, Depth/Stencil Attachment in Vulkan
END_VEX_ENUM_FLAGS();

//clang-format on

enum class ResourceLifetime : u8
{
    Static,  // Lives across many frames.
    Dynamic, // Valid only for the current frame.
};

} // namespace vex