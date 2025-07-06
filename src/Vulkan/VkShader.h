#pragma once

#include <Vex/RHI/RHIShader.h>

namespace vex::vk
{

class VkShader : public RHIShader
{
public:
    VkShader(const ShaderKey& key);
};

} // namespace vex::vk