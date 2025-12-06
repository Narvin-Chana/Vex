#include "DX12ScopedGPUEvent.h"

#include <WinPixEventRuntime/pix3.h>

#include <DX12/RHI/DX12CommandList.h>

namespace vex::dx12
{

DX12ScopedGPUEvent::DX12ScopedGPUEvent(NonNullPtr<DX12CommandList> commandList,
                                       const char* label,
                                       std::array<float, 3> color)
    : RHIScopedGPUEventBase(commandList, label, color)
{
    if (GEnableGPUScopedEvents)
    {
        PIXBeginEvent(commandList->GetNativeCommandList().Get(),
                      PIX_COLOR(static_cast<BYTE>(color[0] * 255.0f),
                                static_cast<BYTE>(color[1] * 255.0f),
                                static_cast<BYTE>(color[2] * 255.0f)),
                      label);
    }
}

DX12ScopedGPUEvent::~DX12ScopedGPUEvent()
{
    if (emitMarker && GEnableGPUScopedEvents)
    {
        PIXEndEvent(commandList->GetNativeCommandList().Get());
    }
}

} // namespace vex::dx12