#pragma once

#include <array>

#include <RHI/RHIFwd.h>
#include <RHI/RHIScopedGPUEvent.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ScopedGPUEvent final : public RHIScopedGPUEventBase
{
public:
    DX12ScopedGPUEvent(NonNullPtr<DX12CommandList> commandList, const char* label, std::array<float, 3> color);
    DX12ScopedGPUEvent(DX12ScopedGPUEvent&&) = default;
    DX12ScopedGPUEvent& operator=(DX12ScopedGPUEvent&&) = default;
    ~DX12ScopedGPUEvent();
};

} // namespace vex::dx12
