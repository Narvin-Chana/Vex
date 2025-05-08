#pragma once

#include <Vex/RHI/RHIResourceLayout.h>

namespace vex::vk
{

class VkResourceLayout : public RHIResourceLayout
{
public:
    virtual bool ValidateGlobalConstant(const GlobalConstant& globalConstant) const override;
    virtual u32 GetMaxLocalConstantSize() const override;
};

} // namespace vex::vk