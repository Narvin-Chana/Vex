#pragma once

#include <Vex/RHIFwd.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

class RHICommandPoolBase
{
public:
    virtual RHICommandList* CreateCommandList(CommandQueueType queueType) = 0;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) = 0;
    virtual void ReclaimAllCommandListMemory() = 0;
};

} // namespace vex