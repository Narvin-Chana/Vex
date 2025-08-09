#include "DX12Fence.h"

#include <Vex/Logger.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{

DX12Fence::DX12Fence() = default;

DX12Fence::DX12Fence(u32 numFenceIndices, ComPtr<DX12Device>& device)
    : fenceValues(numFenceIndices)
{
    chk << device->CreateFence(fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    ++fenceValues[0];

    // Create an event handle to use for frame synchronization.
    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fenceEvent == nullptr || fenceEvent == INVALID_HANDLE_VALUE)
    {
        chk << HRESULT_FROM_WIN32(GetLastError());
    }
}

DX12Fence::~DX12Fence()
{
    if (fenceEvent != nullptr)
    {
        CloseHandle(fenceEvent);
    }
}

u64 DX12Fence::GetCompletedFenceValue() const
{
    VEX_LOG(Info, "Completed fence value: {}", fence->GetCompletedValue());
    return fence->GetCompletedValue();
}

u64 DX12Fence::GetMaxFenceValue() const
{
    return *std::max_element(fenceValues.begin(), fenceValues.end());
}

u64 DX12Fence::GetFenceValue(u32 fenceIndex) const
{
    return fenceValues[fenceIndex];
}

void DX12Fence::WaitCPU(u64 value) const
{
    // Only wait if the current completed fence value is lower than the desired value.
    if (GetCompletedFenceValue() < value)
    {
        VEX_LOG(Info, "Waiting on value {}", value);
        chk << fence->SetEventOnCompletion(value, fenceEvent);
        WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
    }
}

void DX12Fence::WaitCPUAndIncrementNextFenceIndex(u32 currentIndex, u32 nextIndex)
{
    WaitCPU(GetFenceValue(nextIndex));
    fenceValues[nextIndex] = GetFenceValue(currentIndex) + 1;
    VEX_LOG(Info, "Incremented index {} to value {}", nextIndex, fenceValues[nextIndex]);
}

#if !VEX_SHIPPING
void DX12Fence::DumpFenceState() const
{
    VEX_LOG(Info, "Fence:");
    for (u32 i = 0; i < fenceValues.size(); i++)
    {
        VEX_LOG(Info, "\tIndex {}: Value={}, Completed={}", i, GetFenceValue(i), GetCompletedFenceValue());
    }
}
#endif

} // namespace vex::dx12