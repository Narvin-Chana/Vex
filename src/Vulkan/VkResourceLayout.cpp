#include "VkResourceLayout.h"

#include <Vex/Debug.h>

namespace vex::vk
{
bool VkResourceLayout::ValidateGlobalConstant(const GlobalConstant& globalConstant) const
{
    if (!RHIResourceLayout::ValidateGlobalConstant(globalConstant))
    {
        return false;
    }

    VEX_NOT_YET_IMPLEMENTED();
    return true;
}
u32 VkResourceLayout::GetMaxLocalConstantSize() const
{
    VEX_NOT_YET_IMPLEMENTED();
    return 128;
}
} // namespace vex::vk