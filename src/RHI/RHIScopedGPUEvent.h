#pragma once
#include <array>
#include <string>
#include <string_view>

namespace vex
{

inline bool GEnableGPUScopedEvents = false;

class RHIScopedGPUEventBase
{
protected:
    RHIScopedGPUEventBase(std::string_view label, std::array<float, 3> color)
        : emitMarker{ true }
        , label{ label }
        , color{ color }
    {
    }

    RHIScopedGPUEventBase(const RHIScopedGPUEventBase&) = delete;
    RHIScopedGPUEventBase& operator=(RHIScopedGPUEventBase&) = delete;

    RHIScopedGPUEventBase(RHIScopedGPUEventBase&& other) noexcept
        : emitMarker{ other.emitMarker }
        , label{ std::move(other.label) }
        , color{ other.color }
    {
        other.color = { 0, 0, 0 };
        other.emitMarker = false;
    }

    RHIScopedGPUEventBase& operator=(RHIScopedGPUEventBase&& other) noexcept
    {
        emitMarker = other.emitMarker;
        label = std::move(other.label);
        color = other.color;

        other.color = { 0, 0, 0 };
        other.emitMarker = false;
        return *this;
    }

    bool emitMarker;
    std::string label;
    std::array<float, 3> color;
};

} // namespace vex