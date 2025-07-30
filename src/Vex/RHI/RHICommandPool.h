#pragma once

#include <RHIFwd.h>
#include <Vex/RHI/RHI.h>
#include <Vex/UniqueHandle.h>

namespace vex
{

// class RHICommandList;

class RHICommandPool
{
public:
    virtual ~RHICommandPool() = default;
    virtual RHICommandList* CreateCommandList(CommandQueueType queueType) = 0;
    virtual void ReclaimCommandListMemory(CommandQueueType queueType) = 0;
    virtual void ReclaimAllCommandListMemory() = 0;
};

} // namespace vex