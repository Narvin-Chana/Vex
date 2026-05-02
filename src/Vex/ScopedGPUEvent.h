#pragma once

#include <Vex/RHIImpl/RHIScopedGPUEvent.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class ScopedGPUEvent
{
    ScopedGPUEvent(RHIScopedGPUEvent&& marker)
        : marker{ std::move(marker) }
    {
    }
    RHIScopedGPUEvent marker;
    friend class CommandContext;
};

} // namespace vex
