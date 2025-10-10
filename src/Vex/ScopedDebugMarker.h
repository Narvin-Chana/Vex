#pragma once

#include <Vex/RHIImpl/RHIScopedDebugMarker.h>

#include <RHI/RHIFwd.h>

namespace vex
{

class ScopedDebugMarker
{
    ScopedDebugMarker(RHIScopedDebugMarker&& marker)
        : marker{ std::move(marker) }
    {
    }
    RHIScopedDebugMarker marker;
    friend class CommandContext;
};

} // namespace vex

#define COMBINE1(X, Y) X##Y // helper macro
#define COMBINE(X, Y) COMBINE1(X, Y)
#define VEX_GPU_SCOPED_DEBUG_REGION(ctx, name)                                                                         \
    auto COMBINE(zzz_vex_debug_marker_, __LINE__) = ctx.CreateScopedDebugMarker(name);
#define VEX_GPU_SCOPED_DEBUG_REGION_COLORED(ctx, name, r, b, g)                                                        \
    auto COMBINE(zzz_vex_debug_marker_, __LINE__) = ctx.CreateScopedDebugMarker(name, { r, g, b });