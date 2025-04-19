#pragma once

#include <Vex/RHI/RHIShader.h>

namespace vex::vk
{

class VkShader : public RHIShader
{
public:
    VkShader(const ShaderKey& key);
    virtual std::span<const u8> GetBlob() const override;
};

} // namespace vex::vk