#include "RHIFence.h"

#include <Vex/Logger.h>

namespace vex
{

RHIFenceInterface::RHIFenceInterface(u32 numFenceIndices)
    : fenceValues(numFenceIndices)
{
}

u64& RHIFenceInterface::GetFenceValue(u32 fenceIndex)
{
    return fenceValues[fenceIndex];
}

#if !VEX_SHIPPING
void RHIFenceInterface::DumpFenceState()
{
    VEX_LOG(Info, "Fence:");
    for (u32 i = 0; i < fenceValues.size(); i++)
    {
        VEX_LOG(Info, "\tIndex {}: Value={}, Completed={}", i, GetFenceValue(i), GetCompletedFenceValue());
    }
}
#endif

} // namespace vex