#include "DX12ScopedDebugMarker.h"

#include <pix.h>

#include <Vex/Types.h>

namespace vex::dx12
{

DX12ScopedDebugMarker::DX12ScopedDebugMarker(const ComPtr<ID3D12GraphicsCommandList>& commandList,
                                             const char* label,
                                             std::array<float, 3> color)
    : RHIScopedDebugMarkerBase(label, color)
    , cmdList{ commandList }
{
    // Color/Metadata is not used on Windows so 0
    PIXBeginEvent(cmdList.Get(), 0, label);
}

DX12ScopedDebugMarker::~DX12ScopedDebugMarker()
{
    if (emitMarker)
    {
        PIXEndEvent(cmdList.Get());
    }
}

} // namespace vex::dx12