#include "VkShader.h"

#include <Vex/Debug.h>

namespace vex::vk
{

VkShader::VkShader(const ShaderKey& key)
    : RHIShader(key)
{
}

} // namespace vex::vk