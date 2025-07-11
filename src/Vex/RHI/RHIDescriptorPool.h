#pragma once

#include "Vex/Hash.h"

#include <Vex/Handle.h>
#include <Vex/RHI/RHIFwd.h>

namespace vex
{

class RHIDescriptorPool
{
public:
    virtual ~RHIDescriptorPool() = default;
};

} // namespace vex
