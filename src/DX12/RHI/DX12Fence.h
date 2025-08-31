#pragma once

#include <Vex/Types.h>

#include <RHI/RHIFence.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence : public RHIFenceBase
{
public:
    DX12Fence(ComPtr<DX12Device>& device);
    ~DX12Fence();

    virtual u64 GetValue() const override;
    // Blocks CPU until the GPU signals the requested fence value.
    virtual void WaitOnCPU(u64 value) const override;
    virtual void SignalOnCPU(u64 value) override;

    ComPtr<ID3D12Fence1> fence;
};

} // namespace vex::dx12