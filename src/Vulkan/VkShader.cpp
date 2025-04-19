#include "VkShader.h"

#include <Vex/Debug.h>

namespace vex::vk
{

VkShader::VkShader(const ShaderKey& key)
    : RHIShader(key)
{
}

std::span<const u8> VkShader::GetBlob() const
{
    VEX_NOT_YET_IMPLEMENTED();
    return std::span<const u8>();
}

} // namespace vex::vk