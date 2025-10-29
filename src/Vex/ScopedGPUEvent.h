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

#if !VEX_SHIPPING

#define COMBINE1(X, Y) X##Y // helper macro
#define COMBINE(X, Y) COMBINE1(X, Y)

#define VEX_GPU_SCOPED_EVENT(ctx, name)                                                                                \
    auto COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedDebugMarker(name);
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)                                                                   \
    auto COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedDebugMarker(name, { r, g, b });

#else

#define VEX_GPU_SCOPED_EVENT(ctx, name)
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)

#endif