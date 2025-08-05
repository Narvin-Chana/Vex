#include "RHIFence.h"

#include <Vex/Logger.h>

namespace vex
{

RHIFenceBase::RHIFenceBase(u32 numFenceIndices)
    : fenceValues(numFenceIndices)
{
}

u64& RHIFenceBase::GetFenceValue(u32 fenceIndex)
{
    return fenceValues[fenceIndex];
}

#if !VEX_SHIPPING
void RHIFenceBase::DumpFenceState()
{
    VEX_LOG(Info, "Fence:");
    for (u32 i = 0; i < fenceValues.size(); i++)
    {
        VEX_LOG(Info, "\tIndex {}: Value={}, Completed={}", i, GetFenceValue(i), GetCompletedFenceValue());
    }
}
#endif

} // namespace vex