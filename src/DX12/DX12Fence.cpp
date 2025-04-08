#include "DX12Fence.h"

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12Fence::DX12Fence(u32 numFenceIndices, ComPtr<DX12Device>& device)
    : RHIFence(numFenceIndices)
{
    chk << device->CreateFence(GetFenceValue(0), D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    ++GetFenceValue(0);

    // Create an event handle to use for frame synchronization.
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr)
    {
        chk << HRESULT_FROM_WIN32(GetLastError());
    }
}

void DX12Fence::WaitCPU(u32 index)
{
    chk << fence->SetEventOnCompletion(GetFenceValue(index), fenceEvent);
    WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
}

u64 DX12Fence::GetCompletedFenceValue()
{
    return fence->GetCompletedValue();
}

} // namespace vex::dx12