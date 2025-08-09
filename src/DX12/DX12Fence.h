#pragma once

#include <vector>

#include <Vex/Types.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12Fence
{
public:
    DX12Fence(ComPtr<DX12Device>& device);
    ~DX12Fence();

    // Blocks CPU until the GPU signals the requested fence value.
    void WaitCPU(u64 value) const;

    // Blocks the CPU until all GPU signals have been completed.
    void Flush() const;

    u64 nextSignalValue = 1;
    ComPtr<ID3D12Fence1> fence;
};

} // namespace vex::dx12