#pragma once
#include <array>
#include <string>

#include <RHI/RHIScopedDebugMarker.h>

#include <DX12/DX12Headers.h>

namespace vex::dx12
{

class DX12ScopedDebugMarker final : public RHIScopedDebugMarkerBase
{
public:
    DX12ScopedDebugMarker(const ComPtr<ID3D12GraphicsCommandList>& commandList,
                          const char* label,
                          std::array<float, 3> color);
    DX12ScopedDebugMarker(DX12ScopedDebugMarker&&) = default;
    DX12ScopedDebugMarker& operator=(DX12ScopedDebugMarker&&) = default;
    ~DX12ScopedDebugMarker();

private:
    ComPtr<ID3D12GraphicsCommandList> cmdList;
};

} // namespace vex::dx12
