#include "RHIScopedGPUEvent.h"

#include <Vex/Platform/Debug.h>
#include <Vex/RHIImpl/RHICommandList.h>

namespace vex
{

RHIScopedGPUEventBase::RHIScopedGPUEventBase(NonNullPtr<RHICommandList> commandList,
                                             std::string_view label,
                                             std::array<float, 3> color)
    : commandList{ commandList }
    , emitMarker{ true }
    , label{ label }
    , color{ color }
{
}

RHIScopedGPUEventBase::~RHIScopedGPUEventBase()
{
    VEX_CHECK(commandList->IsOpen(),
              "Error: The passed in command list is already closed! Make sure to only submit your command list once "
              "all VEX_GPU_SCOPED_EVENTs are destroyed.");
}

RHIScopedGPUEventBase::RHIScopedGPUEventBase(RHIScopedGPUEventBase&& other)
    : commandList{ other.commandList }
    , emitMarker{ other.emitMarker }
    , label{ std::move(other.label) }
    , color{ other.color }
{
    other.color = { 0, 0, 0 };
    other.emitMarker = false;
}

RHIScopedGPUEventBase& RHIScopedGPUEventBase::operator=(RHIScopedGPUEventBase&& other)
{
    commandList = other.commandList;
    emitMarker = other.emitMarker;
    label = std::move(other.label);
    color = other.color;

    other.color = { 0, 0, 0 };
    other.emitMarker = false;
    return *this;
}

} // namespace vex