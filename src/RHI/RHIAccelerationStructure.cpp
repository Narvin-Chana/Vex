#include "RHIAccelerationStructure.h"

namespace vex
{

void RHIAccelerationStructureBase::FreeBindlessHandles(RHIDescriptorPool& descriptorPool)
{
    if (accelerationStructure.has_value())
    {
        accelerationStructure->FreeBindlessHandles(descriptorPool);
    }
}

void RHIAccelerationStructureBase::FreeAllocation(RHIAllocator& allocator)
{
    if (accelerationStructure.has_value())
    {
        accelerationStructure->FreeAllocation(allocator);
    }
}

} // namespace vex