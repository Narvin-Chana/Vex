#pragma once

#include <Vex/RHIFwd.h>
#include <Vex/NonNullPtr.h>

namespace vex
{

class RHICommandPoolBase
{
public:
    virtual NonNullPtr<RHICommandList> CreateCommandList(CommandQueueType queueType) = 0;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) = 0;
    virtual void ReclaimAllCommandListMemory() = 0;
};

} // namespace vex