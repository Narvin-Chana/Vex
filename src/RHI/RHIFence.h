#pragma once

#include <vector>

#include <Vex/Types.h>

namespace vex
{

class RHIFenceInterface
{
public:
    RHIFenceInterface(u32 numFenceIndices);
    u64& GetFenceValue(u32 fenceIndex);

#if !VEX_SHIPPING
    void DumpFenceState();
#endif

    virtual u64 GetCompletedFenceValue() = 0;
    // CPU-side wait for the index to be signaled by the GPU (this operation blocks the CPU)
    void WaitCPU(u32 index)
    {
        // Only wait if the current completed fence value is lower than the desired value.
        if (GetCompletedFenceValue() < GetFenceValue(index))
        {
            WaitCPU_Internal(index);
        }
    }
    // CPU-side wait for the next index to be signaled by the GPU (this operation blocks the CPU), also sets the
    // nextIndex's value to be the currentIndex's value plus one.
    void WaitCPUAndIncrementNextFenceIndex(u32 currentIndex, u32 nextIndex)
    {
        WaitCPU(nextIndex);
        GetFenceValue(nextIndex) = GetFenceValue(currentIndex) + 1;
    }

protected:
    virtual void WaitCPU_Internal(u32 index) = 0;

private:
    std::vector<u64> fenceValues;
};

} // namespace vex