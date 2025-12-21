#include "VkAccelerationStructure.h"

namespace vex::vk
{

VkAccelerationStructure::VkAccelerationStructure(const ASDesc& desc)
    : RHIAccelerationStructureBase(desc)
{
    VEX_NOT_YET_IMPLEMENTED();
}

const RHIAccelerationStructureBuildInfo& VkAccelerationStructure::SetupBLASBuild(RHIAllocator& allocator,
                                                                                 const RHIBLASBuildDesc& desc)
{
    VEX_NOT_YET_IMPLEMENTED();
    static RHIAccelerationStructureBuildInfo bi{};
    return bi;
}

const RHIAccelerationStructureBuildInfo& VkAccelerationStructure::SetupTLASBuild(RHIAllocator& allocator,
                                                                                 const RHITLASBuildDesc& desc)
{
    VEX_NOT_YET_IMPLEMENTED();
    static RHIAccelerationStructureBuildInfo bi{};
    return bi;
}

} // namespace vex::vk