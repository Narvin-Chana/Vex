#include "DX12Fence.h"

#include <Vex/Logger.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{
// I have no idea why, but if I store the handle in each fence class, we eventually crash due to the handle being marked
// invalid by WinAPI (possibly due to not being able to wait on multiple different handles right after each-other when
// we're iterating on fences?). My current solution is to just create the handle as static... this somehow works
// perfectly...
static HANDLE GEventHandle = nullptr;

DX12Fence::DX12Fence(ComPtr<DX12Device>& device)
{
    chk << device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    if (GEventHandle == nullptr)
    {
        GEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }
}

DX12Fence::~DX12Fence()
{
    if (GEventHandle != nullptr)
    {
        CloseHandle(GEventHandle);
        GEventHandle = nullptr;
    }
}

void DX12Fence::WaitCPU(u64 value) const
{
    if (fence->GetCompletedValue() < value)
    {
        chk << fence->SetEventOnCompletion(value, GEventHandle);
        WaitForSingleObjectEx(GEventHandle, INFINITE, false);
    }
}

void DX12Fence::Flush() const
{
    // Wait for the fence value we signaled most recently.
    WaitCPU(nextSignalValue - 1);
}

} // namespace vex::dx12