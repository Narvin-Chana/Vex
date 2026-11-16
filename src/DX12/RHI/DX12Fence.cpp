#include "DX12Fence.h"

#include <utility>

#include <Vex/Logger.h>

#include <DX12/HRChecker.h>

namespace vex::dx12
{
DX12Fence::DX12Fence(ComPtr<DX12Device>& device)
    : eventHandle(CreateEvent(nullptr, FALSE, FALSE, nullptr))
{
    chk << device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
}

DX12Fence::~DX12Fence()
{
    if (eventHandle != nullptr)
    {
        CloseHandle(eventHandle);
    }
}

DX12Fence::DX12Fence(DX12Fence&& other) noexcept
    : RHIFenceBase(other)
    , fence(std::move(other.fence))
    , eventHandle(std::exchange(other.eventHandle, nullptr))
{
}

DX12Fence& DX12Fence::operator=(DX12Fence&& other) noexcept
{
    if (this != &other)
    {
        RHIFenceBase::operator=(other);
        fence = std::move(other.fence);
        std::swap(eventHandle, other.eventHandle);
    }
    return *this;
}

u64 DX12Fence::GetValue() const
{
    return fence->GetCompletedValue();
}

void DX12Fence::WaitOnCPU(u64 value) const
{
    if (GetValue() < value)
    {
        chk << fence->SetEventOnCompletion(value, eventHandle);
        WaitForSingleObjectEx(eventHandle, INFINITE, false);
    }
}

void DX12Fence::SignalOnCPU(u64 value)
{
    fence->Signal(value);
}

} // namespace vex::dx12