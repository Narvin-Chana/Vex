#pragma once
#include <array>
#include <string>
#include <string_view>

namespace vex
{

class RHIScopedDebugMarkerBase
{
protected:
    RHIScopedDebugMarkerBase(std::string_view label, std::array<float, 3> color)
        : emitMarker{ true }
        , label{ label }
        , color{ color }
    {
    }

    RHIScopedDebugMarkerBase(const RHIScopedDebugMarkerBase&) = delete;
    RHIScopedDebugMarkerBase& operator=(RHIScopedDebugMarkerBase&) = delete;

    RHIScopedDebugMarkerBase(RHIScopedDebugMarkerBase&& other) noexcept
        : emitMarker{ other.emitMarker }
        , label{ std::move(other.label) }
        , color{ other.color }
    {
        other.color = { 0, 0, 0 };
        other.emitMarker = false;
    }

    RHIScopedDebugMarkerBase& operator=(RHIScopedDebugMarkerBase&& other) noexcept
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