#pragma once

#include <Vex/Types.h>

#include <RHI/RHIFence.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence final : public RHIFenceBase
{
public:
    DX12Fence(ComPtr<DX12Device>& device);
    ~DX12Fence();

    DX12Fence(const DX12Fence&) = delete;
    DX12Fence& operator=(const DX12Fence&) = delete;

    DX12Fence(DX12Fence&& other) noexcept;
    DX12Fence& operator=(DX12Fence&& other) noexcept;

    virtual u64 GetValue() const override;
    // Blocks CPU until the GPU signals the requested fence value.
    virtual void WaitOnCPU(u64 value) const override;
    virtual void SignalOnCPU(u64 value) override;

    ComPtr<ID3D12Fence1> fence;

private:
    HANDLE eventHandle = nullptr;
};

} // namespace vex::dx12