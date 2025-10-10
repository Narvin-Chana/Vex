#include "DX12ScopedDebugMarker.h"

#include <WinPixEventRuntime/pix3.h>

namespace vex::dx12
{

DX12ScopedDebugMarker::DX12ScopedDebugMarker(const ComPtr<ID3D12GraphicsCommandList>& commandList,
                                             const char* label,
                                             std::array<float, 3> color)
    : RHIScopedDebugMarkerBase(label, color)
    , cmdList{ commandList }
{
    PIXBeginEvent(cmdList.Get(),
                  PIX_COLOR(static_cast<BYTE>(color[0] * 255.0f),
                            static_cast<BYTE>(color[1] * 255.0f),
                            static_cast<BYTE>(color[2] * 255.0f)),
                  label);
}

DX12ScopedDebugMarker::~DX12ScopedDebugMarker()
{
    if (emitMarker)
    {
        PIXEndEvent(cmdList.Get());
    }
}

} // namespace vex::dx12