#pragma once

#include <Vex/RHI/RHIFwd.h>

namespace vex
{

struct RHIAccessor
{
    // Temporary solution until we rework the RHI system to no longer use reintepret_cast-ing.
    virtual ~RHIAccessor()
    {
    }
};

} // namespace vex