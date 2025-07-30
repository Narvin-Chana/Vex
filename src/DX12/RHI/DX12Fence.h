#pragma once

#include <RHI/RHIFence.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence final : public RHIFenceInterface
{
public:
    DX12Fence(u32 numFenceIndices, ComPtr<DX12Device>& device);
    ~DX12Fence();

    virtual u64 GetCompletedFenceValue() override;

    ComPtr<ID3D12Fence1> fence;

private:
    virtual void WaitCPU_Internal(u32 index) override;

    HANDLE fenceEvent;
};

} // namespace vex::dx12