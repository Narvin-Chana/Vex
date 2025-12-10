#pragma once

#include <Vex/AccelerationStructure.h>

namespace vex
{

class RHIAccelerationStructureBase
{
public:
    RHIAccelerationStructureBase(const ASDesc& desc)
        : desc(desc)
    {
    }

protected:
    ASDesc desc;
};

} // namespace vex