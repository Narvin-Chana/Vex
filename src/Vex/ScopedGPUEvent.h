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

#define VEX_COMBINE1(X, Y) X##Y // helper macro
#define VEX_COMBINE(X, Y) VEX_COMBINE1(X, Y)

#define VEX_GPU_SCOPED_EVENT(ctx, name)                                                                                \
    auto VEX_COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedGPUEvent(name);
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)                                                                   \
    auto VEX_COMBINE(zzz_vex_debug_marker_, __COUNTER__) = ctx.CreateScopedGPUEvent(name, { r, g, b });

#else

#define VEX_GPU_SCOPED_EVENT(ctx, name)
#define VEX_GPU_SCOPED_EVENT_COL(ctx, name, r, b, g)

#endif