#pragma once
#include <array>

#include <RHI/RHIScopedGPUEvent.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ScopedGPUEvent final : public RHIScopedGPUEventBase
{
public:
    DX12ScopedGPUEvent(const ComPtr<ID3D12GraphicsCommandList>& commandList,
                       const char* label,
                       std::array<float, 3> color);
    DX12ScopedGPUEvent(DX12ScopedGPUEvent&&) = default;
    DX12ScopedGPUEvent& operator=(DX12ScopedGPUEvent&&) = default;
    ~DX12ScopedGPUEvent();

private:
    ComPtr<ID3D12GraphicsCommandList> cmdList;
};

} // namespace vex::dx12
