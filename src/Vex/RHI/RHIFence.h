#pragma once

#include <vector>

#include <Vex/Types.h>

namespace vex
{

class RHIFence
{
public:
    RHIFence(u32 numFenceIndices);
    u64& GetFenceValue(u32 fenceIndex);

#if !VEX_SHIPPING
    void DumpFenceState();
#endif

    virtual ~RHIFence() = default;
    virtual u64 GetCompletedFenceValue() = 0;
    // CPU-side wait for the fence to be signaled by the GPU (this operation blocks the CPU)
    virtual void WaitCPU(u32 index) = 0;
    void ConditionalWaitCPUAndIncrementNextFenceIndex(u32 currentIndex, u32 nextIndex)
    {
        // Only wait if the current completed fence value is lower than the desired value.
        if (GetCompletedFenceValue() < GetFenceValue(nextIndex))
        {
            WaitCPU(nextIndex);
        }
        GetFenceValue(nextIndex) = GetFenceValue(currentIndex) + 1;
    }

private:
    std::vector<u64> fenceValues;
};

} // namespace vex