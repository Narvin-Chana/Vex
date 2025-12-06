#pragma once

#include <array>
#include <string>
#include <string_view>

#include <Vex/Utility/NonNullPtr.h>

#include <RHI/RHIFwd.h>

namespace vex
{

inline bool GEnableGPUScopedEvents = false;

class RHIScopedGPUEventBase
{
protected:
    RHIScopedGPUEventBase(NonNullPtr<RHICommandList> commandList, std::string_view label, std::array<float, 3> color);
    ~RHIScopedGPUEventBase();

    RHIScopedGPUEventBase(const RHIScopedGPUEventBase&) = delete;
    RHIScopedGPUEventBase& operator=(RHIScopedGPUEventBase&) = delete;

    RHIScopedGPUEventBase(RHIScopedGPUEventBase&& other);
    RHIScopedGPUEventBase& operator=(RHIScopedGPUEventBase&& other);

    NonNullPtr<RHICommandList> commandList;
    bool emitMarker;
    std::string label;
    std::array<float, 3> color;
};

} // namespace vex