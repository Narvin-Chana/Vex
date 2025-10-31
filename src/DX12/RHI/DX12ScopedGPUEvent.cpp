#include "DX12ScopedGPUEvent.h"

#include <WinPixEventRuntime/pix3.h>

namespace vex::dx12
{

DX12ScopedGPUEvent::DX12ScopedGPUEvent(const ComPtr<ID3D12GraphicsCommandList>& commandList,
                                             const char* label,
                                             std::array<float, 3> color)
    : RHIScopedGPUEventBase(label, color)
    , cmdList{ commandList }
{
    PIXBeginEvent(cmdList.Get(),
                  PIX_COLOR(static_cast<BYTE>(color[0] * 255.0f),
                            static_cast<BYTE>(color[1] * 255.0f),
                            static_cast<BYTE>(color[2] * 255.0f)),
                  label);
}

DX12ScopedGPUEvent::~DX12ScopedGPUEvent()
{
    if (emitMarker)
    {
        PIXEndEvent(cmdList.Get());
    }
}

} // namespace vex::dx12