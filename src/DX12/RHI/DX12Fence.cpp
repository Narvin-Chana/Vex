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

u64 DX12Fence::GetValue() const
{
    return fence->GetCompletedValue();
}

void DX12Fence::WaitOnCPU(u64 value) const
{
    if (GetValue() < value)
    {
        chk << fence->SetEventOnCompletion(value, GEventHandle);
        WaitForSingleObjectEx(GEventHandle, INFINITE, false);
    }
}

void DX12Fence::SignalOnCPU(u64 value)
{
    fence->Signal(value);
}

} // namespace vex::dx12