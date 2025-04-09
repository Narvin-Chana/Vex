#pragma once

#include <Vex/RHI/RHIFence.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence : public RHIFence
{
public:
    DX12Fence(u32 numFenceIndices, ComPtr<DX12Device>& device);

    virtual ~DX12Fence() = default;
    virtual u64 GetCompletedFenceValue() override;

    ComPtr<ID3D12Fence1> fence;

private:
    virtual void WaitCPU_Internal(u32 index) override;

    HANDLE fenceEvent;
};

} // namespace vex::dx12