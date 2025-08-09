#pragma once

#include <vector>

#include <Vex/Types.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence final
{
public:
    DX12Fence();
    DX12Fence(u32 numFenceIndices, ComPtr<DX12Device>& device);
    ~DX12Fence();

    u64 GetFenceValue(u32 fenceIndex) const;
    u64 GetCompletedFenceValue() const;
    u64 GetMaxFenceValue() const;

    // CPU-side wait for the value to be signaled by the GPU (this operation blocks the CPU)
    void WaitCPU(u64 value) const;
    // CPU-side wait for the next index to be signaled by the GPU (this operation blocks the CPU), also sets the
    // nextIndex's value to be the currentIndex's value plus one.
    void WaitCPUAndIncrementNextFenceIndex(u32 currentIndex, u32 nextIndex);

#if !VEX_SHIPPING
    void DumpFenceState() const;
#endif

    ComPtr<ID3D12Fence1> fence;

private:
    std::vector<u64> fenceValues;
    HANDLE fenceEvent;
};

} // namespace vex::dx12